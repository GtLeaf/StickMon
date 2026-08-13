#!/usr/bin/env python3

import argparse
import csv
import hashlib
import io
import json
import subprocess
import sys
import tempfile
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROSTER_PATH = ROOT / "tools" / "pokemon_data" / "species_roster.json"
SNAPSHOT_PATH = ROOT / "tools" / "pokemon_data" / "oras_learnsets.json"
GENERATED_PATH = ROOT / "src" / "game" / "OfficialMoveData.inc"
REPORT_PATH = ROOT / "tools" / "pokemon_data" / "oras_unsupported_moves.csv"
CACHE_ROOT = Path(tempfile.gettempdir()) / "stickmon-pokeapi-cache-v1"
API_ROOT = "https://pokeapi.co/api/v2"
USER_AGENT = "StickMon-data-import/1.0 (+local build-time cache)"
SNAPSHOT_SCHEMA = 2
MAX_WORKERS = 4
FETCH_ATTEMPTS = 4
FULLWIDTH_ALNUM_TO_ASCII = str.maketrans({
    codepoint: codepoint - 0xFEE0
    for start, end in ((0xFF10, 0xFF19), (0xFF21, 0xFF3A), (0xFF41, 0xFF5A))
    for codepoint in range(start, end + 1)
})

TYPE_CPP = {
    "normal": "NORMAL",
    "fire": "FIRE",
    "water": "WATER",
    "grass": "GRASS",
    "electric": "ELECTRIC",
    "ice": "ICE",
    "fighting": "FIGHTING",
    "poison": "POISON",
    "ground": "GROUND",
    "flying": "FLYING",
    "psychic": "PSYCHIC",
    "bug": "BUG",
    "rock": "ROCK",
    "ghost": "GHOST",
    "dragon": "DRAGON",
    "dark": "DARK",
    "steel": "STEEL",
    "fairy": "FAIRY",
}

DAMAGE_CLASS_CPP = {
    "physical": "PHYSICAL",
    "special": "SPECIAL",
    "status": "STATUS",
}

MAJOR_STATUS_CPP = {
    "poison": "POISON",
    "paralysis": "PARALYSIS",
    "sleep": "SLEEP",
    "burn": "BURN",
    "freeze": "FREEZE",
}

BATTLE_STAT_CPP = {
    "attack": "ATTACK",
    "defense": "DEFENSE",
    "special-attack": "SP_ATTACK",
    "special-defense": "SP_DEFENSE",
    "speed": "SPEED",
    "accuracy": "ACCURACY",
    "evasion": "EVASION",
}

# Their modern PokeAPI records include effects that did not exist in ORAS.
ORAS_STAT_CHANGE_OVERRIDES = {
    "rapid-spin": [],
}

# These two species intentionally keep their canonical non-damaging first move.
BASIC_MOVE_OVERRIDES = {
    11: 106,   # Metapod: Harden
    129: 150,  # Magikarp: Splash
    130: 44,   # Gyarados: Bite after evolving
}

# These moves require persistent PP or held-item state, neither of which exists
# in the current battle model. Keep them out of learnsets instead of silently
# reducing them to ordinary damage/status moves.
REMOVED_SYSTEM_DEPENDENT_MOVES = {
    "acrobatics", "bug-bite", "covet", "embargo", "fling", "heal-block",
    "natural-gift", "spite", "thief", "trump-card",
}

SUPPORTED_DAMAGING_MOVES_WITHOUT_GENERIC_RULES = {
    "assurance", "bounce", "brine", "dragon-tail", "dream-eater",
    "eruption", "explosion", "false-swipe", "freeze-dry", "frost-breath",
    "fury-cutter", "giga-impact", "hex", "hyper-beam", "last-resort",
    "outrage", "payback", "petal-dance", "razor-wind", "rollout",
    "self-destruct", "skull-bash", "snore", "stored-power", "sucker-punch",
    "venoshock", "chip-away", "solar-beam",
}

# Known damaging moves whose distinctive behavior cannot yet be represented by
# the compact 1v1 battle state. They remain in source data but cannot be learnt.
UNSUPPORTED_SPECIAL_DAMAGE_MOVES = {
    "future-sight", "pursuit",
}

MOVE_FLAG_IDENTIFIERS = {
    "snore": ["MOVE_FLAG_USABLE_ASLEEP", "MOVE_FLAG_REQUIRES_ASLEEP_USER"],
    "dream-eater": ["MOVE_FLAG_REQUIRES_SLEEPING_TARGET"],
    "solar-beam": ["MOVE_FLAG_TWO_TURN_CHARGE"],
    "razor-wind": ["MOVE_FLAG_TWO_TURN_CHARGE"],
    "skull-bash": ["MOVE_FLAG_TWO_TURN_CHARGE", "MOVE_FLAG_CHARGE_DEFENSE"],
    "bounce": ["MOVE_FLAG_TWO_TURN_CHARGE"],
    "hyper-beam": ["MOVE_FLAG_RECHARGE"],
    "giga-impact": ["MOVE_FLAG_RECHARGE"],
    "self-destruct": ["MOVE_FLAG_SELF_FAINT"],
    "explosion": ["MOVE_FLAG_SELF_FAINT"],
    "false-swipe": ["MOVE_FLAG_FALSE_SWIPE"],
    "thrash": ["MOVE_FLAG_RAMPAGE"],
    "petal-dance": ["MOVE_FLAG_RAMPAGE"],
    "outrage": ["MOVE_FLAG_RAMPAGE"],
    "rollout": ["MOVE_FLAG_ROLLOUT"],
    "fury-cutter": ["MOVE_FLAG_FURY_CUTTER"],
    "venoshock": ["MOVE_FLAG_DOUBLE_POISONED"],
    "hex": ["MOVE_FLAG_DOUBLE_STATUS"],
    "stored-power": ["MOVE_FLAG_STORED_POWER"],
    "eruption": ["MOVE_FLAG_ERUPTION_POWER"],
    "brine": ["MOVE_FLAG_BRINE"],
    "payback": ["MOVE_FLAG_PAYBACK"],
    "assurance": ["MOVE_FLAG_ASSURANCE"],
    "sucker-punch": ["MOVE_FLAG_SUCKER_PUNCH"],
    "chip-away": ["MOVE_FLAG_IGNORE_DEFENDER_STAGES"],
    "dragon-tail": ["MOVE_FLAG_FORCE_WILD_END"],
    "freeze-dry": ["MOVE_FLAG_FREEZE_DRY"],
    "frost-breath": ["MOVE_FLAG_ALWAYS_CRITICAL"],
    "stockpile": ["MOVE_FLAG_STOCKPILE"],
    "swallow": ["MOVE_FLAG_SWALLOW"],
    "spit-up": ["MOVE_FLAG_SPIT_UP"],
    "poison-powder": ["MOVE_FLAG_POWDER"],
    "stun-spore": ["MOVE_FLAG_POWDER"],
    "sleep-powder": ["MOVE_FLAG_POWDER"],
    "spore": ["MOVE_FLAG_POWDER"],
}

# Physical moves in the current reachable move set that don't make contact.
NON_CONTACT_PHYSICAL_MOVES = {
    "earthquake", "explosion", "ice-shard", "pin-missile", "razor-leaf",
    "rock-blast", "rock-slide", "rock-throw", "self-destruct", "stone-edge",
}


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_text(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


def fetch_json(url, cache_path):
    if cache_path.exists():
        try:
            return read_json(cache_path)
        except (OSError, json.JSONDecodeError):
            cache_path.unlink(missing_ok=True)

    last_error = None
    for attempt in range(FETCH_ATTEMPTS):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(request, timeout=45) as response:
                value = json.load(response)
                write_text(cache_path, json.dumps(value, ensure_ascii=False))
                return value
        except Exception as error:
            last_error = error
            if attempt + 1 < FETCH_ATTEMPTS:
                time.sleep(0.75 * (2 ** attempt))

    curl = subprocess.run(
        [
            "curl",
            "-fsSL",
            "--retry",
            "5",
            "--retry-all-errors",
            "--connect-timeout",
            "15",
            "--max-time",
            "90",
            "-A",
            USER_AGENT,
            url,
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if curl.returncode == 0:
        value = json.loads(curl.stdout)
        write_text(cache_path, json.dumps(value, ensure_ascii=False))
        return value
    raise RuntimeError(
        f"failed to fetch {url}: urllib={last_error}; curl={curl.stderr.strip()}"
    ) from last_error


def localized_name(resource, language="zh-hans"):
    for value in resource.get("names", []):
        if value.get("language", {}).get("name") == language:
            return value.get("name", "")
    for value in resource.get("names", []):
        if value.get("language", {}).get("name") == "en":
            return value.get("name", "")
    return resource.get("name", "")


def normalized_text(value):
    value = " ".join((value or "").replace("\f", " ").split())
    return value.translate(FULLWIDTH_ALNUM_TO_ASCII)


def localized_description(resource, version_group, version_group_id):
    entries = resource.get("flavor_text_entries", [])
    chinese = [
        entry for entry in entries
        if entry.get("language", {}).get("name") == "zh-hans" and
        resource_id(entry.get("version_group", {})) >= version_group_id
    ]
    if chinese:
        closest = min(
            chinese,
            key=lambda entry: resource_id(entry.get("version_group", {})),
        )
        return normalized_text(closest.get("flavor_text", ""))

    exact_english = [
        entry for entry in entries
        if entry.get("language", {}).get("name") == "en" and
        entry.get("version_group", {}).get("name") == version_group
    ]
    if exact_english:
        return normalized_text(exact_english[0].get("flavor_text", ""))

    english = [
        entry for entry in entries
        if entry.get("language", {}).get("name") == "en"
    ]
    if english:
        return normalized_text(english[-1].get("flavor_text", ""))
    return ""


def normalized_effect_chance(value, direct=False):
    value = int(value or 0)
    return value if value > 0 else (100 if direct else 0)


def derive_move_effects(move):
    effects = []
    meta = move.get("meta") or {}
    identifier = move.get("identifier", "")
    damage_class = move.get("damageClass", "status")
    direct = damage_class == "status"
    ailment = meta.get("ailment", "none")

    if ailment in MAJOR_STATUS_CPP:
        status = "TOXIC" if identifier in ("toxic", "poison-fang") else MAJOR_STATUS_CPP[ailment]
        effects.append({
            "kind": "MAJOR_STATUS",
            "target": "DEFENDER",
            "chance": normalized_effect_chance(
                meta.get("ailmentChance") or move.get("effectChance"), direct),
            "value": status,
            "aux": 0,
            "minTurns": int(meta.get("minTurns") or 0),
            "maxTurns": int(meta.get("maxTurns") or 0),
        })
    elif ailment == "confusion":
        effects.append({
            "kind": "CONFUSION",
            "target": "DEFENDER",
            "chance": normalized_effect_chance(
                meta.get("ailmentChance") or move.get("effectChance"), direct),
            "value": 0,
            "aux": 0,
            "minTurns": int(meta.get("minTurns") or 2),
            "maxTurns": int(meta.get("maxTurns") or 5),
        })
    elif ailment == "trap":
        effects.append({
            "kind": "BIND",
            "target": "DEFENDER",
            "chance": normalized_effect_chance(
                meta.get("ailmentChance") or move.get("effectChance"), True),
            "value": 0,
            "aux": 0,
            "minTurns": 2,
            "maxTurns": 5,
        })
    elif ailment == "yawn":
        effects.append({
            "kind": "YAWN",
            "target": "DEFENDER",
            "chance": 100,
            "value": 0,
            "aux": 0,
            "minTurns": 0,
            "maxTurns": 0,
        })

    flinch_chance = int(meta.get("flinchChance") or 0)
    if flinch_chance > 0:
        effects.append({
            "kind": "FLINCH", "target": "DEFENDER", "chance": flinch_chance,
            "value": 0, "aux": 0, "minTurns": 0, "maxTurns": 0,
        })

    category = meta.get("category", "")
    stat_changes = ORAS_STAT_CHANGE_OVERRIDES.get(identifier, move.get("statChanges", []))
    if category not in ("unique", "whole-field-effect", "field-effect"):
        if category == "damage-raise":
            stat_target = "ATTACKER"
        elif category in ("damage-lower", "swagger"):
            stat_target = "DEFENDER"
        else:
            stat_target = None
        for change in stat_changes:
            stat_name = change.get("stat")
            amount = int(change.get("change") or 0)
            if stat_name not in BATTLE_STAT_CPP or amount == 0:
                continue
            target = stat_target or ("ATTACKER" if amount > 0 else "DEFENDER")
            effects.append({
                "kind": "STAT_STAGE",
                "target": target,
                "chance": normalized_effect_chance(meta.get("statChance"), direct),
                "value": amount,
                "aux": BATTLE_STAT_CPP[stat_name],
                "minTurns": 0,
                "maxTurns": 0,
            })

    drain = int(meta.get("drain") or 0)
    if drain > 0:
        effects.append({
            "kind": "DRAIN", "target": "ATTACKER", "chance": 100,
            "value": min(drain, 100), "aux": 0, "minTurns": 0, "maxTurns": 0,
        })
    elif drain < 0:
        effects.append({
            "kind": "RECOIL", "target": "ATTACKER", "chance": 100,
            "value": min(-drain, 100), "aux": 0, "minTurns": 0, "maxTurns": 0,
        })

    healing = int(meta.get("healing") or 0)
    target = move.get("target", "")
    if healing > 0 and target == "user":
        effects.append({
            "kind": "HEAL", "target": "ATTACKER", "chance": 100,
            "value": min(healing, 100), "aux": 0, "minTurns": 0, "maxTurns": 0,
        })

    if identifier == "rest":
        effects.append({
            "kind": "REST", "target": "ATTACKER", "chance": 100,
            "value": 0, "aux": 0, "minTurns": 2, "maxTurns": 2,
        })
    elif identifier in ("refresh", "heal-bell", "aromatherapy"):
        effects.append({
            "kind": "CURE_STATUS", "target": "ATTACKER", "chance": 100,
            "value": 0, "aux": 0, "minTurns": 0, "maxTurns": 0,
        })

    if identifier == "rapid-spin":
        effects.append({
            "kind": "CLEAR_BIND", "target": "ATTACKER", "chance": 100,
            "value": 0, "aux": 0, "minTurns": 0, "maxTurns": 0,
        })
    if identifier == "swallow":
        effects.append({
            "kind": "HEAL", "target": "ATTACKER", "chance": 100,
            "value": 25, "aux": 0, "minTurns": 0, "maxTurns": 0,
        })
    if identifier == "stockpile":
        for stat in ("DEFENSE", "SP_DEFENSE"):
            effects.append({
                "kind": "STAT_STAGE", "target": "ATTACKER", "chance": 100,
                "value": 1, "aux": stat, "minTurns": 0, "maxTurns": 0,
            })
    return effects


def move_is_battle_supported(move):
    identifier = move.get("identifier", "")
    if identifier in REMOVED_SYSTEM_DEPENDENT_MOVES or \
            identifier in UNSUPPORTED_SPECIAL_DAMAGE_MOVES:
        return False
    damaging = (
        move.get("damageClass") in ("physical", "special")
        and int(move.get("power") or 0) > 0
    )
    if not damaging:
        return bool(derive_move_effects(move)) or identifier in (
            "spit-up", "stockpile", "swallow")
    category = (move.get("meta") or {}).get("category", "damage")
    if category in ("unique", "whole-field-effect", "field-effect"):
        return identifier in SUPPORTED_DAMAGING_MOVES_WITHOUT_GENERIC_RULES
    return identifier not in UNSUPPORTED_SPECIAL_DAMAGE_MOVES


def selected_level_moves(pokemon, version_group):
    selected = []
    for move_index, move in enumerate(pokemon.get("moves", [])):
        for detail in move.get("version_group_details", []):
            if detail.get("version_group", {}).get("name") != version_group:
                continue
            if detail.get("move_learn_method", {}).get("name") != "level-up":
                continue
            selected.append({
                "level": int(detail.get("level_learned_at") or 0),
                "order": detail.get("order"),
                "moveIdentifier": move.get("move", {}).get("name", ""),
                "sourceIndex": move_index,
            })
    selected.sort(key=lambda entry: (
        entry["level"],
        entry["order"] if entry["order"] is not None else 255,
        entry["sourceIndex"],
        entry["moveIdentifier"],
    ))
    return selected


def resource_id(resource):
    url = resource.get("url", "").rstrip("/")
    try:
        return int(url.rsplit("/", 1)[-1])
    except (TypeError, ValueError):
        return 0


def move_values_for_version(resource, version_group_id):
    values = {
        "power": resource.get("power"),
        "accuracy": resource.get("accuracy"),
        "pp": resource.get("pp"),
        "effectChance": resource.get("effect_chance"),
        "type": resource.get("type", {}).get("name", "normal"),
    }
    future_boundaries = []
    for past in resource.get("past_values", []):
        boundary = resource_id(past.get("version_group", {}))
        if boundary > version_group_id:
            future_boundaries.append((boundary, past))
    if not future_boundaries:
        return values

    for _, past in sorted(future_boundaries, key=lambda item: item[0], reverse=True):
        for field, source_field in (
            ("power", "power"),
            ("accuracy", "accuracy"),
            ("pp", "pp"),
            ("effectChance", "effect_chance"),
        ):
            if past.get(source_field) is not None:
                values[field] = past[source_field]
        if past.get("type"):
            values["type"] = past["type"].get("name", values["type"])
    return values


def fetch_many(kind, identifiers):
    results = {}
    identifiers = list(identifiers)
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = {
            executor.submit(
                fetch_json,
                f"{API_ROOT}/{kind}/{identifier}",
                CACHE_ROOT / kind / f"{identifier}.json",
            ): identifier
            for identifier in identifiers
        }
        completed = 0
        for future in as_completed(futures):
            identifier = futures[future]
            results[identifier] = future.result()
            completed += 1
            if completed == len(identifiers) or completed % 20 == 0:
                print(f"[pokeapi] {kind}: {completed}/{len(identifiers)}", file=sys.stderr)
    return results


def derive_basic_move(species_id, entries, move_by_id):
    entry_ids = {entry["moveId"] for entry in entries}
    override = BASIC_MOVE_OVERRIDES.get(species_id)
    if override in entry_ids:
        return override

    damaging = [
        entry for entry in entries
        if move_by_id[entry["moveId"]].get("damageClass") in ("physical", "special")
        and int(move_by_id[entry["moveId"]].get("power") or 0) > 0
    ]
    if not damaging:
        if entries:
            return entries[0]["moveId"]
        raise RuntimeError(f"species {species_id} has no ORAS level-up moves")

    level_one = [entry for entry in damaging if entry["level"] <= 1]
    candidates = level_one or damaging
    if level_one:
        candidates.sort(key=lambda entry: (
            int(move_by_id[entry["moveId"]].get("power") or 0),
            entry["order"] if entry["order"] is not None else 255,
            entry["moveId"],
        ))
    else:
        candidates.sort(key=lambda entry: (
            entry["level"],
            int(move_by_id[entry["moveId"]].get("power") or 0),
            entry["moveId"],
        ))
    return candidates[0]["moveId"]


def refresh_snapshot(roster):
    version_group = roster["versionGroup"]
    species_ids = sorted(set(int(value) for value in roster["speciesIds"]))
    version_group_resource = fetch_json(
        f"{API_ROOT}/version-group/{version_group}",
        CACHE_ROOT / "version-group" / f"{version_group}.json",
    )
    version_group_id = int(version_group_resource["id"])
    pokemon_resources = fetch_many("pokemon", species_ids)
    species_resources = fetch_many("pokemon-species", species_ids)

    raw_species = []
    move_identifiers = set()
    for species_id in species_ids:
        pokemon = pokemon_resources[species_id]
        entries = selected_level_moves(pokemon, version_group)
        if not entries:
            raise RuntimeError(
                f"species {species_id} ({pokemon.get('name')}) has no {version_group} learnset"
            )
        move_identifiers.update(entry["moveIdentifier"] for entry in entries)
        raw_species.append({
            "id": species_id,
            "identifier": pokemon.get("name", str(species_id)),
            "evolvesFromId": resource_id(
                species_resources[species_id].get("evolves_from_species") or {}
            ),
            "entries": entries,
        })

    move_resources = fetch_many("move", sorted(move_identifiers))
    moves = []
    move_id_by_identifier = {}
    for identifier, resource in move_resources.items():
        move_id = int(resource["id"])
        move_id_by_identifier[identifier] = move_id
        damage_class = resource.get("damage_class", {}).get("name", "status")
        version_values = move_values_for_version(resource, version_group_id)
        raw_meta = resource.get("meta") or {}
        move = {
            "id": move_id,
            "identifier": identifier,
            "name": localized_name(resource),
            "description": localized_description(resource, version_group, version_group_id),
            "power": int(version_values.get("power") or 0),
            "type": version_values.get("type", "normal"),
            "damageClass": damage_class,
            "accuracy": int(version_values.get("accuracy") or 0),
            "pp": int(version_values.get("pp") or 0),
            "priority": int(resource.get("priority") or 0),
            "effectChance": int(version_values.get("effectChance") or 0),
            "target": resource.get("target", {}).get("name", "selected-pokemon"),
            "meta": {
                "ailment": raw_meta.get("ailment", {}).get("name", "none"),
                "category": raw_meta.get("category", {}).get("name", "damage"),
                "ailmentChance": int(raw_meta.get("ailment_chance") or 0),
                "flinchChance": int(raw_meta.get("flinch_chance") or 0),
                "statChance": int(raw_meta.get("stat_chance") or 0),
                "drain": int(raw_meta.get("drain") or 0),
                "healing": int(raw_meta.get("healing") or 0),
                "criticalStage": int(raw_meta.get("crit_rate") or 0),
                "minHits": int(raw_meta.get("min_hits") or 0),
                "maxHits": int(raw_meta.get("max_hits") or 0),
                "minTurns": int(raw_meta.get("min_turns") or 0),
                "maxTurns": int(raw_meta.get("max_turns") or 0),
            },
            "statChanges": [
                {
                    "stat": change.get("stat", {}).get("name", ""),
                    "change": int(change.get("change") or 0),
                }
                for change in resource.get("stat_changes", [])
            ],
        }
        move["battleSupported"] = move_is_battle_supported(move)
        moves.append(move)
    moves.sort(key=lambda move: move["id"])
    move_by_id = {move["id"]: move for move in moves}

    normalized_by_id = {}
    for raw in raw_species:
        entries = []
        seen = set()
        for entry in raw["entries"]:
            move_id = move_id_by_identifier[entry["moveIdentifier"]]
            duplicate_key = (entry["level"], move_id)
            if duplicate_key in seen:
                continue
            seen.add(duplicate_key)
            entries.append({
                "level": entry["level"],
                "order": entry["order"],
                "moveId": move_id,
            })
        normalized_by_id[raw["id"]] = {
            "id": raw["id"],
            "identifier": raw["identifier"],
            "evolvesFromId": raw["evolvesFromId"],
            "ownBasicMoveId": derive_basic_move(raw["id"], entries, move_by_id),
            "learnset": entries,
        }

    def root_species_id(species_id):
        visited = set()
        current = species_id
        while current in normalized_by_id and current not in visited:
            visited.add(current)
            parent = normalized_by_id[current]["evolvesFromId"]
            if parent == 0 or parent not in normalized_by_id:
                return current
            current = parent
        return species_id

    species = []
    for species_id in sorted(normalized_by_id):
        value = normalized_by_id[species_id]
        root = normalized_by_id[root_species_id(species_id)]
        root_basic = int(root["ownBasicMoveId"])
        known_moves = {int(entry["moveId"]) for entry in value["learnset"]}
        basic_move = root_basic if root_basic in known_moves else int(value["ownBasicMoveId"])
        if species_id in BASIC_MOVE_OVERRIDES:
            override = BASIC_MOVE_OVERRIDES[species_id]
            if override in known_moves:
                basic_move = override
        species.append({
            "id": species_id,
            "identifier": value["identifier"],
            "evolvesFromId": value["evolvesFromId"],
            "basicMoveId": basic_move,
            "learnset": value["learnset"],
        })

    return {
        "schema": SNAPSHOT_SCHEMA,
        "source": {
            "provider": "PokeAPI",
            "api": API_ROOT,
            "project": "https://github.com/PokeAPI/pokeapi",
            "versionGroup": version_group,
            "versionGroupId": version_group_id,
            "moveLearnMethod": "level-up",
            "fetchedAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        },
        "species": species,
        "moves": moves,
    }


def validate_snapshot(snapshot, roster):
    if snapshot.get("schema") != SNAPSHOT_SCHEMA:
        raise RuntimeError(f"unsupported snapshot schema: {snapshot.get('schema')}")
    if snapshot.get("source", {}).get("versionGroup") != roster.get("versionGroup"):
        raise RuntimeError("snapshot version group does not match species_roster.json")
    expected = sorted(set(int(value) for value in roster["speciesIds"]))
    actual = sorted(int(species["id"]) for species in snapshot.get("species", []))
    if actual != expected:
        raise RuntimeError("snapshot species list does not match species_roster.json")

    moves = {int(move["id"]): move for move in snapshot.get("moves", [])}
    for move in moves.values():
        if move.get("type") not in TYPE_CPP:
            raise RuntimeError(f"unknown move type: {move.get('type')}")
        if move.get("damageClass") not in DAMAGE_CLASS_CPP:
            raise RuntimeError(f"unknown damage class: {move.get('damageClass')}")
        # battleSupported is derived by the current engine rules. The cached
        # PokeAPI snapshot keeps source metadata stable across generator updates.
    for species in snapshot.get("species", []):
        entries = species.get("learnset", [])
        if not entries:
            raise RuntimeError(f"species {species['id']} has an empty learnset")
        if int(species.get("basicMoveId", 0)) not in moves:
            raise RuntimeError(f"species {species['id']} has an unknown basic move")
        for entry in entries:
            if int(entry["moveId"]) not in moves:
                raise RuntimeError(
                    f"species {species['id']} references unknown move {entry['moveId']}"
                )


def cpp_string(value):
    return json.dumps(value or "", ensure_ascii=False)


def canonical_snapshot_hash(snapshot):
    payload = json.dumps(snapshot, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def effect_value_cpp(effect):
    if effect["kind"] == "MAJOR_STATUS":
        return f"static_cast<int8_t>(Game::MajorStatus::{effect['value']})"
    return str(int(effect["value"]))


def effect_aux_cpp(effect):
    if effect["kind"] == "STAT_STAGE":
        return f"static_cast<uint8_t>(BattleStat::{effect['aux']})"
    return str(int(effect["aux"]))


def render_generated(snapshot):
    snapshot_hash = canonical_snapshot_hash(snapshot)
    sorted_moves = sorted(
        (
            move for move in snapshot["moves"]
            if move.get("identifier") not in REMOVED_SYSTEM_DEPENDENT_MOVES
        ),
        key=lambda value: int(value["id"]),
    )
    move_by_id = {int(move["id"]): move for move in sorted_moves}
    effect_rows = []
    move_effect_ranges = {}
    for move in sorted_moves:
        effects = derive_move_effects(move)
        if len(effects) > 255:
            raise RuntimeError(f"move {move['id']} effect count exceeds uint8_t")
        move_effect_ranges[int(move["id"])] = (len(effect_rows), len(effects))
        effect_rows.extend(effects)
    if len(effect_rows) > 65535:
        raise RuntimeError("combined move effects exceed uint16_t offset")

    lines = [
        "// Generated by tools/import_pokeapi_learnsets.py. Do not edit by hand.",
        f"// Source: PokeAPI {snapshot['source']['versionGroup']} snapshot sha256={snapshot_hash}",
        "",
        "static const MoveEffectSpec OFFICIAL_MOVE_EFFECTS[] = {",
    ]
    for effect in effect_rows:
        lines.append(
            "    {"
            f"MoveEffectKind::{effect['kind']}, "
            f"MoveEffectTarget::{effect['target']}, "
            f"{int(effect['chance'])}, {effect_value_cpp(effect)}, "
            f"{effect_aux_cpp(effect)}, {int(effect['minTurns'])}, "
            f"{int(effect['maxTurns'])}"
            "},"
        )
    lines.extend([
        "};",
        "static constexpr size_t OFFICIAL_MOVE_EFFECT_COUNT =",
        "    sizeof(OFFICIAL_MOVE_EFFECTS) / sizeof(OFFICIAL_MOVE_EFFECTS[0]);",
        "",
        "static const MoveInfo OFFICIAL_MOVES[] = {",
    ])
    for move in sorted_moves:
        effect_offset, effect_count = move_effect_ranges[int(move["id"])]
        meta = move.get("meta") or {}
        min_hits = int(meta.get("minHits") or 1)
        max_hits = int(meta.get("maxHits") or min_hits)
        identifier = move.get("identifier", "")
        move_flags = list(MOVE_FLAG_IDENTIFIERS.get(identifier, ()))
        if (move.get("damageClass") == "physical" and
                identifier not in NON_CONTACT_PHYSICAL_MOVES):
            move_flags.append("MOVE_FLAG_CONTACT")
        flags = " | ".join(move_flags) if move_flags else "MOVE_FLAG_NONE"
        lines.append(
            "    {"
            f"{int(move['id'])}, {cpp_string(move['name'])}, "
            f"{cpp_string(move['description'])}, {int(move['power'])}, "
            f"TypeId::{TYPE_CPP[move['type']]}, "
            f"DamageClass::{DAMAGE_CLASS_CPP[move['damageClass']]}, "
            f"{int(move['accuracy'])}, {int(move['pp'])}, {int(move['priority'])}, "
            f"{str(move_is_battle_supported(move)).lower()}, "
            f"{effect_offset}, {effect_count}, {min_hits}, {max_hits}, "
            f"{int(meta.get('criticalStage') or 0)}, {flags}"
            "},"
        )
    lines.extend([
        "};",
        "static constexpr size_t OFFICIAL_MOVE_COUNT =",
        "    sizeof(OFFICIAL_MOVES) / sizeof(OFFICIAL_MOVES[0]);",
        "",
        "static const LearnsetEntry OFFICIAL_LEARNSET_ENTRIES[] = {",
    ])

    species_rows = []
    offset = 0
    for species in sorted(snapshot["species"], key=lambda value: int(value["id"])):
        entries = [
            entry for entry in species["learnset"]
            if int(entry["moveId"]) in move_by_id
        ]
        if len(entries) > 255:
            raise RuntimeError(f"species {species['id']} learnset exceeds uint8_t count")
        for entry in entries:
            lines.append(f"    {{{int(entry['level'])}, {int(entry['moveId'])}}},")
        species_rows.append((
            int(species["id"]),
            offset,
            len(entries),
            int(species["basicMoveId"]),
        ))
        offset += len(entries)
    if offset > 65535:
        raise RuntimeError("combined learnset exceeds uint16_t offset")

    lines.extend([
        "};",
        "static constexpr size_t OFFICIAL_LEARNSET_ENTRY_COUNT =",
        "    sizeof(OFFICIAL_LEARNSET_ENTRIES) / sizeof(OFFICIAL_LEARNSET_ENTRIES[0]);",
        "",
        "static const SpeciesLearnset OFFICIAL_LEARNSETS[] = {",
    ])
    for species_id, entry_offset, count, basic_move_id in species_rows:
        lines.append(f"    {{{species_id}, {entry_offset}, {count}, {basic_move_id}}},")
    lines.extend([
        "};",
        "static constexpr size_t OFFICIAL_LEARNSET_COUNT =",
        "    sizeof(OFFICIAL_LEARNSETS) / sizeof(OFFICIAL_LEARNSETS[0]);",
        "",
    ])
    return "\n".join(lines)


def render_report(snapshot):
    move_by_id = {int(move["id"]): move for move in snapshot["moves"]}
    output = io.StringIO()
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow([
        "species_id",
        "species",
        "level",
        "move_id",
        "move",
        "name_zh_hans",
        "reason",
    ])
    for species in sorted(snapshot["species"], key=lambda value: int(value["id"])):
        for entry in species["learnset"]:
            move = move_by_id[int(entry["moveId"])]
            if move_is_battle_supported(move):
                continue
            identifier = move.get("identifier", "")
            if identifier in REMOVED_SYSTEM_DEPENDENT_MOVES:
                reason = "requires_pp_or_held_item"
            elif identifier in UNSUPPORTED_SPECIAL_DAMAGE_MOVES:
                reason = "special_battle_rule_not_implemented"
            else:
                reason = "status" if move["damageClass"] == "status" else "variable_or_zero_power"
            writer.writerow([
                species["id"],
                species["identifier"],
                entry["level"],
                move["id"],
                move["identifier"],
                move["name"],
                reason,
            ])
    return output.getvalue()


def update_or_check(path, expected, check):
    current = path.read_text(encoding="utf-8") if path.exists() else None
    if current == expected:
        return True
    if check:
        print(f"stale generated file: {path.relative_to(ROOT)}", file=sys.stderr)
        return False
    write_text(path, expected)
    print(f"generated {path.relative_to(ROOT)}")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Import a pinned ORAS level-up learnset snapshot from PokeAPI"
    )
    parser.add_argument(
        "--refresh",
        action="store_true",
        help="fetch PokeAPI and replace the local snapshot before generating",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail when generated outputs differ from the local snapshot",
    )
    args = parser.parse_args()
    if args.refresh and args.check:
        parser.error("--refresh and --check cannot be used together")

    roster = read_json(ROSTER_PATH)
    if args.refresh:
        snapshot = refresh_snapshot(roster)
        write_text(
            SNAPSHOT_PATH,
            json.dumps(snapshot, ensure_ascii=False, indent=2) + "\n",
        )
        print(f"cached {SNAPSHOT_PATH.relative_to(ROOT)}")
    else:
        if not SNAPSHOT_PATH.exists():
            parser.error("local snapshot missing; run once with --refresh")
        snapshot = read_json(SNAPSHOT_PATH)

    validate_snapshot(snapshot, roster)
    generated = render_generated(snapshot)
    report = render_report(snapshot)
    ok = update_or_check(GENERATED_PATH, generated, args.check)
    ok &= update_or_check(REPORT_PATH, report, args.check)
    print(
        f"ORAS snapshot species={len(snapshot['species'])} "
        f"moves={len(snapshot['moves'])} "
        f"entries={sum(len(value['learnset']) for value in snapshot['species'])}"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
