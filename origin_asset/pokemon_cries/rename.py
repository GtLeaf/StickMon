#!/usr/bin/env python3
import os

name_to_id = {
    'magikarp': '129', 'gyarados': '130', 'eevee': '133', 'vaporeon': '134',
    'jolteon': '135', 'flareon': '136', 'snorlax': '143', 'dratini': '147',
    'dragonair': '148', 'dragonite': '149', 'sentret': '161', 'furret': '162',
    'scizor': '212', 'ralts': '280', 'kirlia': '281', 'numel': '322',
    'camerupt': '323', 'snorunt': '361', 'latias': '380', 'latios': '381',
    'mew': '151', 'crobat': '169', 'pichu': '172', 'marill': '183',
    'azumarill': '184', 'hoppip': '187', 'skiploom': '188', 'jumpluff': '189',
    'wooper': '194', 'quagsire': '195', 'espeon': '196', 'umbreon': '197',
    'poochyena': '261', 'mightyena': '262', 'taillow': '276', 'swellow': '277',
    'wingull': '278', 'pelipper': '279', 'gardevoir': '282', 'shroomish': '285',
    'breloom': '286', 'azurill': '298', 'glalie': '362',
    'bulbasaur': '001', 'ivysaur': '002', 'venusaur': '003', 'charmander': '004',
    'charmeleon': '005', 'charizard': '006', 'squirtle': '007', 'wartortle': '008',
    'blastoise': '009', 'caterpie': '010', 'metapod': '011', 'butterfree': '012',
    'weedle': '013', 'kakuna': '014', 'beedrill': '015', 'pidgey': '016',
    'pidgeotto': '017', 'pidgeot': '018', 'pikachu': '025', 'raichu': '026',
    'zubat': '041', 'golbat': '042', 'geodude': '074', 'graveler': '075',
    'golem': '076', 'gastly': '092', 'haunter': '093', 'gengar': '094',
    'scyther': '123',
}

renamed = 0
skipped = []
for fn in sorted(os.listdir('.')):
    if not fn.endswith('.wav'):
        continue
    base = fn.replace('.wav', '')
    nid = name_to_id.get(base)
    if nid:
        new_fn = f'{nid}_{fn}'
        print(f'Renaming: {fn} -> {new_fn}')
        os.rename(fn, new_fn)
        renamed += 1
    else:
        skipped.append(fn)

print(f'\n成功重命名: {renamed} 个')
if skipped:
    print(f'未匹配: {len(skipped)} 个')
    for s in skipped:
        print('  ', s)
