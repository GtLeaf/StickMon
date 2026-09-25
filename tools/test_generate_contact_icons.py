from pathlib import Path

from generate_contact_icons import NAMES, OUTPUT, SOURCE, SIZE, bitmap, generate


def test_generated_contact_icons_match_sources():
    assert OUTPUT.read_text(encoding="utf-8") == generate()
    for name in NAMES:
        pixels = bitmap(SOURCE / f"{name}.png")
        assert len(pixels) == SIZE * SIZE // 8
        assert sum(value.bit_count() for value in pixels) > 50
        occupied = [index for index in range(SIZE * SIZE)
                    if pixels[index // 8] & (0x80 >> (index % 8))]
        xs = [index % SIZE for index in occupied]
        ys = [index // SIZE for index in occupied]
        assert max(xs) - min(xs) >= SIZE - 12
        assert max(ys) - min(ys) >= SIZE - 12


def test_generated_icon_sources_are_project_local():
    assert SOURCE.is_relative_to(Path(__file__).resolve().parents[1])
