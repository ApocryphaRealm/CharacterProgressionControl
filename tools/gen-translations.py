# -*- coding: utf-8 -*-
"""gen-translations.py - builds the eleven CharacterProgressionControl_<language>.txt files.

ONE file per language, with TWO key sets in it, because this mod has two consumers of the same
file (translation rollout plan, section 4.1):

  * the SWF set - the level-up menu's own keys, kept VERBATIM ($skOne, $skTwo, $SSL_PointsPerLevel
    ...). MenuStrings::Install() reads the file for the GAME's language and adds these to the
    game's Scaleform translator, because that menu is drawn by the game, not by the framework.
    English and Russian are the strings this mod already shipped, unchanged; the other nine
    languages are new and use Skyrim's own localised skill names.
  * the CPC_ set - the settings pages' text, read through include/utils/Strings.h for whatever
    language the Apocrypha Menu Framework reports. The English half is EXTRACTED FROM THE PATCHED
    source/UI.cpp (TR("KEY", "text") calls plus the parallel kFooKeys[]/kFooLabels[] tables), so
    the English file can never drift from the code.

The translator install is harmless for the extra CPC_ keys, and the check script's warning that
the 44 SWF keys are unused by the source is expected.

Writes REPO/dist/Interface/Translations/CharacterProgressionControl_<language>.txt for english +
the ten languages: UTF-16LE with a BOM, one "$key<TAB>text" record per line, a literal "\\n" for an
embedded line break, CRLF records - the SKSE/SkyUI shape the framework's Strings.cpp reads.

Untranslated on purpose, in every language: product and mod names (Skyrim, Character Progression
Control, Apocrypha Menu Framework, Static Skill Leveling Rewritten, Blade and Blunt, Requiem,
Experience, Custom Difficulty UI); file, folder and INI names (BladeAndBlunt.ini,
bLevelBasedDifficulty, CharacterProgressionControl.log and the Documents path); the numeric row
format "%-12s  %5.1f   %.0f / %.0f"; and every printf specifier, kept in the same order as the
English.

Run: `python tools/gen-translations.py`.
"""
import io
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STEM = "CharacterProgressionControl"
LANGS = ["english", "japanese", "korean", "chinese", "russian",
         "german", "french", "spanish", "italian", "polish", "czech"]
OTHER = LANGS[1:]

TR_RE = re.compile(r'strings::TR\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)', re.S)
STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
KEYS_RE = re.compile(r'constexpr const char\* (?:const )?k(\w+)Keys\[\d*\]\s*=\s*\{(.*?)\};', re.S)
LABELS_RE = re.compile(r'constexpr const char\* (?:const )?k(\w+)Labels\[\d*\]\s*=\s*\{(.*?)\};', re.S)


def unescape(s):
    return s.encode("latin-1", "backslashreplace").decode("unicode_escape") if "\\" in s else s


def read_keys():
    src = io.open(os.path.join(REPO, "source", "UI.cpp"), encoding="utf-8", newline="").read()
    order, texts = [], {}

    def add(key, text):
        if key in texts:
            if texts[key] != text:
                raise RuntimeError("key {!r} has two English texts: {!r} vs {!r}".format(key, texts[key], text))
            return
        order.append(key)
        texts[key] = text

    for m in TR_RE.finditer(src):
        add(unescape(m.group(1)), "".join(unescape(x) for x in STR_RE.findall(m.group(2))))

    labels = {m.group(1): m.group(2) for m in LABELS_RE.finditer(src)}
    for m in KEYS_RE.finditer(src):
        name = m.group(1)
        if name not in labels:
            raise RuntimeError("k{}Keys has no matching k{}Labels array".format(name, name))
        ks = [unescape(x) for x in STR_RE.findall(m.group(2))]
        vs = [unescape(x) for x in STR_RE.findall(labels[name])]
        if len(ks) != len(vs):
            raise RuntimeError("k{}Keys ({}) and k{}Labels ({}) length mismatch".format(name, len(ks), name, len(vs)))
        for k, v in zip(ks, vs):
            add(k, v)

    return order, texts


def T(ja, ko, zh, ru, de, fr, es, it, pl, cs):
    return {"japanese": ja, "korean": ko, "chinese": zh, "russian": ru, "german": de,
            "french": fr, "spanish": es, "italian": it, "polish": pl, "czech": cs}


# ================================================================================================
# 1) The SWF set - the level-up menu's keys, in the order the English file already ships them.
#    English and Russian are that file's own strings, unchanged.
# ================================================================================================
SWF_ORDER = [
    "skOne", "skTwo", "skArch", "skBlock", "skSmith", "skHeavy", "skLight", "skPick", "skLock",
    "skSneak", "skAlch", "skSpeech", "skAlter", "skConj", "skDestr", "skIll", "skRes", "skEnch",
    "General", "SkillList", "SSL_PointsPerLevel", "SSL_FinalizeInfo",
    "MaxLevelsPerSkillPerPlayerLevel", "SkillPointCost0", "SkillPointCost25", "SkillPointCost50",
    "SkillPointCost75", "SkillPointsLevelMultiplier", "SkillPointsPerLevel",
    "MaxLevelsPerSkillPerPlayerLevelDesc", "SkillPointCost0Desc", "SkillPointCost25Desc",
    "SkillPointCost50Desc", "SkillPointCost75Desc", "SkillPointsLevelMultiplierDesc",
    "SkillPointsPerLevelDesc", "SSL_EnableDebug", "SSL_EnableDebugDesc", "SSL_SkillPointsCap",
    "SSL_SkillPointsCapDesc", "SSL_SkillCostProgression", "SSL_SkillCostProgressionDesc",
    "SSL_DoDisableSkillsLeveling", "SSL_DoDisableSkillsLevelingDesc",
]

SWF_ENGLISH = {
    "skOne": "One-Handed", "skTwo": "Two-Handed", "skArch": "Marksman", "skBlock": "Block",
    "skSmith": "Smithing", "skHeavy": "Heavy armor", "skLight": "Light armor",
    "skPick": "Pickpocketing", "skLock": "Lockpicking", "skSneak": "Sneak", "skAlch": "Alchemy",
    "skSpeech": "Speechcraft", "skAlter": "Alteration", "skConj": "Conjuration",
    "skDestr": "Destruction", "skIll": "Illusion", "skRes": "Restoration", "skEnch": "Enchanting",
    "General": "General",
    "SkillList": "Active skills",
    "SSL_PointsPerLevel": "Skill points available to distribute:",
    "SSL_FinalizeInfo": "Select an attribute to increase:",
    "MaxLevelsPerSkillPerPlayerLevel": "Skill levelups limit per level",
    "SkillPointCost0": "Skill point cost on 0",
    "SkillPointCost25": "Skill point cost on 25",
    "SkillPointCost50": "Skill point cost on 50",
    "SkillPointCost75": "Skill point cost on 75",
    "SkillPointsLevelMultiplier": "Skill points level multiplier",
    "SkillPointsPerLevel": "Skill points per level",
    "MaxLevelsPerSkillPerPlayerLevelDesc": "The maximum number of skill levels a player can gain per level for one skill",
    "SkillPointCost0Desc": "This is the cost of raising a skill to 25",
    "SkillPointCost25Desc": "This is the cost of raising a skill from 25-50",
    "SkillPointCost50Desc": "This is the cost of raising a skill from 50-75",
    "SkillPointCost75Desc": "This is the cost of raising a skill above 75",
    "SkillPointsLevelMultiplierDesc": "This is the multiplier added to the base number of skillpoints per level (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)",
    "SkillPointsPerLevelDesc": "This is the base number of skillpoints gained per level",
    "SSL_EnableDebug": "Enable debugging",
    "SSL_EnableDebugDesc": "Enable debugging messages to staticskillleveling.log in user log folder (Documents-My games-Skyrim se-Logs)",
    "SSL_SkillPointsCap": "Maximum obtainable skill points",
    "SSL_SkillPointsCapDesc": "Set how many skill points player can gain per level. 0 - no limits, default value. E.G. if limit was set to 50 but by calculations you will take 51 then on Level up Menu you wll recieve 50 and you will lost all skillpoints saved from previous level.",
    "SSL_SkillCostProgression": "Skill Levels Progression",
    "SSL_SkillCostProgressionDesc": "Custom progression curve for amount of skills you can level up per level. Example: 1-5;40-15;. Those numbers means that each level until 40 you can level up one skill 5 times and after 40 level - 15 times. Symbol ; between numbers are required, last ; symbol is optional.",
    "SSL_DoDisableSkillsLeveling": "Toggle skills leveling",
    "SSL_DoDisableSkillsLevelingDesc": "Enable skills leveling back and change SkillMult values in Skyrim Uncapper if you have any problem with this option",
}

# The Russian strings this mod already shipped, kept exactly. The two keys the old Russian file
# named differently ($SSL_SkillLevelsProgression / ...Desc) carry their text over onto the English
# file's key names, and its two extra $skHybridLeveling* keys are dropped - the level-up menu SWF
# uses neither, and every language file has to hold the same key set.
SWF_RUSSIAN = {
    "skOne": "Одноручное оружие", "skTwo": "Двуручное оружие", "skArch": "Стрельба",
    "skBlock": "Блокирование", "skSmith": "Кузнечное дело", "skHeavy": "Тяжелая броня",
    "skLight": "Легкая броня", "skPick": "Карманные кражи", "skLock": "Взлом",
    "skSneak": "Скрытность", "skAlch": "Алхимия", "skSpeech": "Красноречие",
    "skAlter": "Изменение", "skConj": "Колдовство", "skDestr": "Разрушение",
    "skIll": "Иллюзия", "skRes": "Восстановление", "skEnch": "Зачарование",
    "General": "Основное",
    "SkillList": "Активные способности",
    "SSL_PointsPerLevel": "Доступно очков навыков для распределения:",
    "SSL_FinalizeInfo": "Атрибут для повышения:",
    "MaxLevelsPerSkillPerPlayerLevel": "Количество повышений за уровень",
    "SkillPointCost0": "Стоимость навыка после 0",
    "SkillPointCost25": "Стоимость навыка после 25",
    "SkillPointCost50": "Стоимость навыка после 50",
    "SkillPointCost75": "Стоимость навыка после 75",
    "SkillPointsLevelMultiplier": "Множитель получаемых очков навыков",
    "SkillPointsPerLevel": "Базовое число очков навыков",
    "MaxLevelsPerSkillPerPlayerLevelDesc": "Сколько максимум раз за уровень можно вложиться в один навык",
    "SkillPointCost0Desc": "Стоимость повышения навыка до 25",
    "SkillPointCost25Desc": "Стоимость повышения навыка в диапазоне 25-50",
    "SkillPointCost50Desc": "Стоимость повышения навыка в диапазоне 50-75",
    "SkillPointCost75Desc": "Стоимость повышения навыка выше 75",
    "SkillPointsLevelMultiplierDesc": "Множитель количество очков навыков, получаемых за уровень (базовое число + Уровень игрока * множитель)",
    "SkillPointsPerLevelDesc": "Базовое количество очков навыков, получаемое на любом уровне",
    "SSL_EnableDebug": "Включить отладку",
    "SSL_EnableDebugDesc": "Запись отладочных сообщений в staticskillleveling.log в папке журналов (Documents-My games-Skyrim se-Logs)",
    "SSL_SkillPointsCap": "Максимальное число скиллпойнтов за уровень",
    "SSL_SkillPointsCapDesc": "Определяет максимальное значение скиллпойнтов которое будет получено на следующем уровне. 0 - без ограничений, стандартное значение.",
    "SSL_SkillCostProgression": "Прогрессия вложений в навык за уровень",
    "SSL_SkillCostProgressionDesc": "Отдельная настройка для динамичного ограничения прокачки навыков. Пример: 1-5;40-15; значит что до 40 уровня вы можете прокачать один навык 5 раз а после 40 уровня - 15 раз. Символ ; между числами обязателен, в конце ; опционален.",
    "SSL_DoDisableSkillsLeveling": "Навыки получают опыт",
    "SSL_DoDisableSkillsLevelingDesc": "Отключите эту опцию если возникли какие-то проблемы с прокачкой навыков и измените SkillMult поля в SkyrimUncapper.ini",
}

# The nine new languages for the SWF set. Skill names are Skyrim's own localised names.
SWF = {}
SWF["skOne"] = T("片手武器", "한손 무기", "单手武器", SWF_RUSSIAN["skOne"], "Einhändig", "Une main", "Una mano", "Una mano", "Broń jednoręczna", "Jednoruční zbraně")
SWF["skTwo"] = T("両手武器", "양손 무기", "双手武器", SWF_RUSSIAN["skTwo"], "Zweihändig", "Deux mains", "Dos manos", "Due mani", "Broń dwuręczna", "Obouruční zbraně")
SWF["skArch"] = T("弓", "궁술", "射术", SWF_RUSSIAN["skArch"], "Bogenschießen", "Archerie", "Arquería", "Tiro con l'arco", "Łucznictwo", "Lukostřelba")
SWF["skBlock"] = T("受け", "방어", "格挡", SWF_RUSSIAN["skBlock"], "Blocken", "Parade", "Bloqueo", "Parata", "Blokowanie", "Blokování")
SWF["skSmith"] = T("鍛冶", "대장기술", "锻造", SWF_RUSSIAN["skSmith"], "Schmiedekunst", "Forge", "Herrería", "Forgiatura", "Kowalstwo", "Kovářství")
SWF["skHeavy"] = T("重装", "중갑", "重甲", SWF_RUSSIAN["skHeavy"], "Schwere Rüstung", "Armure lourde", "Armadura pesada", "Armatura pesante", "Ciężki pancerz", "Těžká zbroj")
SWF["skLight"] = T("軽装", "경갑", "轻甲", SWF_RUSSIAN["skLight"], "Leichte Rüstung", "Armure légère", "Armadura ligera", "Armatura leggera", "Lekki pancerz", "Lehká zbroj")
SWF["skPick"] = T("スリ", "소매치기", "扒窃", SWF_RUSSIAN["skPick"], "Taschendiebstahl", "Vol à la tire", "Hurto", "Borseggio", "Kradzież kieszonkowa", "Kapsářství")
SWF["skLock"] = T("開錠", "자물쇠 따기", "开锁", SWF_RUSSIAN["skLock"], "Schlossknacken", "Crochetage", "Forzar cerraduras", "Scasso", "Otwieranie zamków", "Páčení zámků")
SWF["skSneak"] = T("隠密", "은신", "潜行", SWF_RUSSIAN["skSneak"], "Schleichen", "Discrétion", "Sigilo", "Furtività", "Skradanie", "Plížení")
SWF["skAlch"] = T("錬金術", "연금술", "炼金术", SWF_RUSSIAN["skAlch"], "Alchemie", "Alchimie", "Alquimia", "Alchimia", "Alchemia", "Alchymie")
SWF["skSpeech"] = T("話術", "화술", "口才", SWF_RUSSIAN["skSpeech"], "Redekunst", "Éloquence", "Oratoria", "Persuasione", "Krasomówstwo", "Výmluvnost")
SWF["skAlter"] = T("変性", "변화", "变换系", SWF_RUSSIAN["skAlter"], "Veränderung", "Altération", "Alteración", "Alterazione", "Zmiany", "Proměny")
SWF["skConj"] = T("召喚", "소환", "召唤系", SWF_RUSSIAN["skConj"], "Beschwörung", "Conjuration", "Conjuración", "Evocazione", "Przywołania", "Vyvolávání")
SWF["skDestr"] = T("破壊", "파괴", "毁灭系", SWF_RUSSIAN["skDestr"], "Zerstörung", "Destruction", "Destrucción", "Distruzione", "Zniszczenie", "Ničení")
SWF["skIll"] = T("幻惑", "환영", "幻术系", SWF_RUSSIAN["skIll"], "Illusion", "Illusion", "Ilusión", "Illusione", "Złudzenia", "Iluze")
SWF["skRes"] = T("回復", "회복", "恢复系", SWF_RUSSIAN["skRes"], "Wiederherstellung", "Restauration", "Restauración", "Ripristino", "Przywracanie", "Obnova")
SWF["skEnch"] = T("付呪", "마법부여", "附魔", SWF_RUSSIAN["skEnch"], "Verzauberung", "Enchantement", "Encantamiento", "Ammaliamento", "Zaklinanie", "Očarování")
SWF["General"] = T("全般", "일반", "常规", SWF_RUSSIAN["General"], "Allgemein", "Général", "General", "Generale", "Ogólne", "Obecné")
SWF["SkillList"] = T("アクティブスキル", "활성 기술", "启用的技能", SWF_RUSSIAN["SkillList"], "Aktive Fertigkeiten", "Compétences actives", "Habilidades activas", "Abilità attive", "Aktywne umiejętności", "Aktivní dovednosti")
SWF["SSL_PointsPerLevel"] = T("割り振れるスキルポイント:", "분배할 수 있는 기술 점수:", "可分配的技能点：", SWF_RUSSIAN["SSL_PointsPerLevel"], "Verteilbare Fertigkeitspunkte:", "Points de compétence à répartir :", "Puntos de habilidad disponibles:", "Punti abilità da distribuire:", "Punkty umiejętności do rozdania:", "Body dovedností k rozdělení:")
SWF["SSL_FinalizeInfo"] = T("上昇させる能力値を選択:", "증가시킬 능력치를 선택하세요:", "选择要提升的属性：", SWF_RUSSIAN["SSL_FinalizeInfo"], "Wähle ein Attribut zum Steigern:", "Choisissez un attribut à augmenter :", "Elige un atributo para aumentar:", "Scegli un attributo da aumentare:", "Wybierz atrybut do zwiększenia:", "Vyber atribut ke zvýšení:")
SWF["MaxLevelsPerSkillPerPlayerLevel"] = T("1レベルあたりのスキル上昇回数の上限", "레벨당 기술 상승 제한", "每级技能提升次数上限", SWF_RUSSIAN["MaxLevelsPerSkillPerPlayerLevel"], "Fertigkeitsaufstiege pro Stufe", "Limite de progressions par niveau", "Límite de subidas por nivel", "Limite di aumenti per livello", "Limit awansów umiejętności na poziom", "Limit zvýšení dovedností za úroveň")
SWF["SkillPointCost0"] = T("スキルポイント消費 (0以上)", "기술 점수 비용 (0 이상)", "技能点消耗（0 起）", SWF_RUSSIAN["SkillPointCost0"], "Punktekosten ab 0", "Coût en points à partir de 0", "Coste de puntos desde 0", "Costo in punti da 0", "Koszt punktów od 0", "Cena bodů od 0")
SWF["SkillPointCost25"] = T("スキルポイント消費 (25以上)", "기술 점수 비용 (25 이상)", "技能点消耗（25 起）", SWF_RUSSIAN["SkillPointCost25"], "Punktekosten ab 25", "Coût en points à partir de 25", "Coste de puntos desde 25", "Costo in punti da 25", "Koszt punktów od 25", "Cena bodů od 25")
SWF["SkillPointCost50"] = T("スキルポイント消費 (50以上)", "기술 점수 비용 (50 이상)", "技能点消耗（50 起）", SWF_RUSSIAN["SkillPointCost50"], "Punktekosten ab 50", "Coût en points à partir de 50", "Coste de puntos desde 50", "Costo in punti da 50", "Koszt punktów od 50", "Cena bodů od 50")
SWF["SkillPointCost75"] = T("スキルポイント消費 (75以上)", "기술 점수 비용 (75 이상)", "技能点消耗（75 起）", SWF_RUSSIAN["SkillPointCost75"], "Punktekosten ab 75", "Coût en points à partir de 75", "Coste de puntos desde 75", "Costo in punti da 75", "Koszt punktów od 75", "Cena bodů od 75")
SWF["SkillPointsLevelMultiplier"] = T("スキルポイントのレベル倍率", "기술 점수 레벨 배수", "技能点等级系数", SWF_RUSSIAN["SkillPointsLevelMultiplier"], "Stufenmultiplikator für Punkte", "Multiplicateur de points par niveau", "Multiplicador de puntos por nivel", "Moltiplicatore punti per livello", "Mnożnik punktów za poziom", "Násobitel bodů za úroveň")
SWF["SkillPointsPerLevel"] = T("1レベルあたりのスキルポイント", "레벨당 기술 점수", "每级技能点", SWF_RUSSIAN["SkillPointsPerLevel"], "Fertigkeitspunkte pro Stufe", "Points de compétence par niveau", "Puntos de habilidad por nivel", "Punti abilità per livello", "Punkty umiejętności na poziom", "Body dovedností za úroveň")
SWF["MaxLevelsPerSkillPerPlayerLevelDesc"] = T("1レベルの間に1つのスキルを上げられる最大回数", "한 레벨 동안 한 기술을 올릴 수 있는 최대 횟수", "每提升一级，单个技能最多可提升的次数", SWF_RUSSIAN["MaxLevelsPerSkillPerPlayerLevelDesc"], "Wie oft eine einzelne Fertigkeit pro Stufe steigen darf", "Nombre maximal de progressions d'une même compétence par niveau", "Cuántas veces puede subir una misma habilidad por nivel", "Quante volte una singola abilità può salire per livello", "Ile razy jedna umiejętność może wzrosnąć na poziom", "Kolikrát smí jedna dovednost stoupnout za úroveň")
SWF["SkillPointCost0Desc"] = T("スキルを25まで上げる際の消費", "기술을 25까지 올리는 비용", "将技能提升至 25 的花费", SWF_RUSSIAN["SkillPointCost0Desc"], "Kosten, um eine Fertigkeit auf 25 zu steigern", "Coût pour monter une compétence jusqu'à 25", "Coste de subir una habilidad hasta 25", "Costo per portare un'abilità a 25", "Koszt podniesienia umiejętności do 25", "Cena zvýšení dovednosti na 25")
SWF["SkillPointCost25Desc"] = T("スキルを25から50まで上げる際の消費", "기술을 25에서 50까지 올리는 비용", "将技能从 25 提升至 50 的花费", SWF_RUSSIAN["SkillPointCost25Desc"], "Kosten von 25 bis 50", "Coût de 25 à 50", "Coste de 25 a 50", "Costo da 25 a 50", "Koszt od 25 do 50", "Cena od 25 do 50")
SWF["SkillPointCost50Desc"] = T("スキルを50から75まで上げる際の消費", "기술을 50에서 75까지 올리는 비용", "将技能从 50 提升至 75 的花费", SWF_RUSSIAN["SkillPointCost50Desc"], "Kosten von 50 bis 75", "Coût de 50 à 75", "Coste de 50 a 75", "Costo da 50 a 75", "Koszt od 50 do 75", "Cena od 50 do 75")
SWF["SkillPointCost75Desc"] = T("スキルを75より上に上げる際の消費", "기술을 75 이상으로 올리는 비용", "将技能提升至 75 以上的花费", SWF_RUSSIAN["SkillPointCost75Desc"], "Kosten über 75 hinaus", "Coût au-delà de 75", "Coste por encima de 75", "Costo oltre 75", "Koszt powyżej 75", "Cena nad 75")
SWF["SkillPointsLevelMultiplierDesc"] = T("レベルごとのスキルポイント基本値に加える倍率 (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)", "레벨당 기본 기술 점수에 더해지는 배수 (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)", "在每级基础技能点上叠加的系数（SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier）", SWF_RUSSIAN["SkillPointsLevelMultiplierDesc"], "Multiplikator zusätzlich zur Grundzahl der Punkte pro Stufe (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)", "Multiplicateur ajouté au nombre de base de points par niveau (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)", "Multiplicador añadido al número base de puntos por nivel (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)", "Moltiplicatore aggiunto al numero base di punti per livello (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)", "Mnożnik dodawany do bazowej liczby punktów na poziom (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)", "Násobitel přičtený k základnímu počtu bodů za úroveň (SkillPointsPerLevel + PlayerLevel * SkillPointsLevelMultiplier)")
SWF["SkillPointsPerLevelDesc"] = T("レベルごとに得られるスキルポイントの基本値", "레벨마다 얻는 기본 기술 점수", "每级获得的基础技能点", SWF_RUSSIAN["SkillPointsPerLevelDesc"], "Grundzahl der Fertigkeitspunkte pro Stufe", "Nombre de base de points de compétence par niveau", "Número base de puntos de habilidad por nivel", "Numero base di punti abilità per livello", "Bazowa liczba punktów umiejętności na poziom", "Základní počet bodů dovedností za úroveň")
SWF["SSL_EnableDebug"] = T("デバッグを有効化", "디버깅 활성화", "启用调试", SWF_RUSSIAN["SSL_EnableDebug"], "Debugging aktivieren", "Activer le débogage", "Activar la depuración", "Attiva il debug", "Włącz debugowanie", "Zapnout ladění")
SWF["SSL_EnableDebugDesc"] = T("ユーザーのログフォルダ (Documents-My games-Skyrim se-Logs) の staticskillleveling.log にデバッグメッセージを記録します", "사용자 로그 폴더(Documents-My games-Skyrim se-Logs)의 staticskillleveling.log에 디버그 메시지를 기록합니다", "将调试信息写入用户日志文件夹（Documents-My games-Skyrim se-Logs）中的 staticskillleveling.log", SWF_RUSSIAN["SSL_EnableDebugDesc"], "Schreibt Debug-Meldungen in staticskillleveling.log im Protokollordner (Documents-My games-Skyrim se-Logs)", "Écrit les messages de débogage dans staticskillleveling.log du dossier de journaux (Documents-My games-Skyrim se-Logs)", "Escribe mensajes de depuración en staticskillleveling.log en la carpeta de registros (Documents-My games-Skyrim se-Logs)", "Scrive i messaggi di debug in staticskillleveling.log nella cartella dei log (Documents-My games-Skyrim se-Logs)", "Zapisuje komunikaty debugowania do staticskillleveling.log w folderze dzienników (Documents-My games-Skyrim se-Logs)", "Zapisuje ladicí zprávy do staticskillleveling.log ve složce protokolů (Documents-My games-Skyrim se-Logs)")
SWF["SSL_SkillPointsCap"] = T("獲得できるスキルポイントの上限", "획득 가능한 최대 기술 점수", "可获得的技能点上限", SWF_RUSSIAN["SSL_SkillPointsCap"], "Höchstzahl erhaltbarer Fertigkeitspunkte", "Maximum de points de compétence obtenables", "Máximo de puntos de habilidad obtenibles", "Massimo di punti abilità ottenibili", "Maksymalna liczba zdobywanych punktów", "Maximum získatelných bodů dovedností")
SWF["SSL_SkillPointsCapDesc"] = T("1レベルで得られるスキルポイントの上限を決めます。0 は無制限 (既定値) です。", "레벨당 얻을 수 있는 기술 점수의 상한을 정합니다. 0은 제한 없음(기본값)입니다.", "设定每级可获得的技能点上限。0 表示不限制（默认值）。", SWF_RUSSIAN["SSL_SkillPointsCapDesc"], "Legt fest, wie viele Fertigkeitspunkte pro Stufe höchstens vergeben werden. 0 - keine Begrenzung, Standardwert.", "Définit le nombre maximal de points de compétence obtenus par niveau. 0 - aucune limite, valeur par défaut.", "Define cuántos puntos de habilidad se obtienen como máximo por nivel. 0 - sin límite, valor predeterminado.", "Definisce quanti punti abilità si ottengono al massimo per livello. 0 - nessun limite, valore predefinito.", "Określa, ile punktów umiejętności można zdobyć na poziom. 0 - bez ograniczeń, wartość domyślna.", "Určuje, kolik bodů dovedností lze získat za úroveň. 0 - bez omezení, výchozí hodnota.")
SWF["SSL_SkillCostProgression"] = T("スキル上昇回数の推移", "기술 상승 진행 곡선", "技能提升次数曲线", SWF_RUSSIAN["SSL_SkillCostProgression"], "Verlauf der Fertigkeitsaufstiege", "Progression des montées de compétence", "Progresión de subidas de habilidad", "Progressione degli aumenti di abilità", "Progresja awansów umiejętności", "Průběh zvyšování dovedností")
SWF["SSL_SkillCostProgressionDesc"] = T("1レベルで上げられるスキル回数の独自カーブ。例: 1-5;40-15; は40レベルまで1つのスキルを5回、40レベル以降は15回上げられるという意味です。数字の間の ; は必須で、末尾の ; は省略できます。", "레벨당 올릴 수 있는 기술 횟수의 사용자 곡선. 예: 1-5;40-15; 은 40레벨까지 한 기술을 5회, 40레벨 이후에는 15회 올릴 수 있다는 뜻입니다. 숫자 사이의 ; 는 필수이고 마지막 ; 는 선택입니다.", "自定义每级可提升技能次数的曲线。例如 1-5;40-15; 表示 40 级前每级可将一个技能提升 5 次，40 级之后为 15 次。数字之间的 ; 必填，末尾的 ; 可省略。", SWF_RUSSIAN["SSL_SkillCostProgressionDesc"], "Eigene Kurve dafür, wie viele Fertigkeitsaufstiege pro Stufe möglich sind. Beispiel: 1-5;40-15; - bis Stufe 40 fünfmal, ab Stufe 40 fünfzehnmal. Das ; zwischen den Zahlen ist Pflicht, das letzte ; ist optional.", "Courbe personnalisée du nombre de montées de compétence par niveau. Exemple : 1-5;40-15; - cinq fois jusqu'au niveau 40, quinze fois ensuite. Le ; entre les nombres est obligatoire, le dernier ; est facultatif.", "Curva personalizada del número de subidas de habilidad por nivel. Ejemplo: 1-5;40-15; - cinco veces hasta el nivel 40 y quince después. El ; entre números es obligatorio; el último ; es opcional.", "Curva personalizzata del numero di aumenti di abilità per livello. Esempio: 1-5;40-15; - cinque volte fino al livello 40 e quindici dopo. Il ; tra i numeri è obbligatorio, l'ultimo ; è facoltativo.", "Własna krzywa liczby awansów umiejętności na poziom. Przykład: 1-5;40-15; - pięć razy do poziomu 40, potem piętnaście. ; między liczbami jest wymagany, ostatni ; jest opcjonalny.", "Vlastní křivka počtu zvýšení dovedností za úroveň. Příklad: 1-5;40-15; - pětkrát do úrovně 40 a poté patnáctkrát. Znak ; mezi čísly je povinný, poslední ; je nepovinný.")
SWF["SSL_DoDisableSkillsLeveling"] = T("スキルの経験取得を切り替え", "기술 경험 획득 전환", "切换技能升级", SWF_RUSSIAN["SSL_DoDisableSkillsLeveling"], "Fertigkeitsaufstieg umschalten", "Activer ou non la progression des compétences", "Alternar la subida de habilidades", "Attiva o disattiva la crescita delle abilità", "Przełącz rozwój umiejętności", "Přepnout zvyšování dovedností")
SWF["SSL_DoDisableSkillsLevelingDesc"] = T("スキルの経験取得を元に戻し、問題がある場合は Skyrim Uncapper の SkillMult 値を変更してください", "기술 경험 획득을 되돌리고, 문제가 있으면 Skyrim Uncapper의 SkillMult 값을 변경하세요", "恢复技能升级；若仍有问题，请修改 Skyrim Uncapper 中的 SkillMult 数值", SWF_RUSSIAN["SSL_DoDisableSkillsLevelingDesc"], "Schaltet den Fertigkeitsaufstieg wieder ein; ändere bei Problemen die SkillMult-Werte im Skyrim Uncapper", "Réactive la progression des compétences ; en cas de problème, modifiez les valeurs SkillMult dans Skyrim Uncapper", "Vuelve a activar la subida de habilidades; si hay problemas, cambia los valores SkillMult en Skyrim Uncapper", "Riattiva la crescita delle abilità; in caso di problemi cambia i valori SkillMult in Skyrim Uncapper", "Ponownie włącza rozwój umiejętności; w razie problemów zmień wartości SkillMult w Skyrim Uncapper", "Znovu zapne zvyšování dovedností; při potížích změň hodnoty SkillMult ve Skyrim Uncapperu")


# ================================================================================================
# 2) The CPC_ set - the settings pages' own text, one dict per key, in the order the pages draw it.
# ================================================================================================
TRANSLATIONS = {}

# --- shared: the three buttons every tab carries, and the shared status lines --------------------
TRANSLATIONS["CPC_Btn_Save"] = T(
    "保存", "저장", "保存", "Сохранить", "Speichern", "Enregistrer", "Guardar", "Salva", "Zapisz", "Uložit")
TRANSLATIONS["CPC_Status_Saving"] = T(
    "保存中...", "저장 중...", "正在保存…", "Сохранение...", "Speichern ...", "Enregistrement...",
    "Guardando...", "Salvataggio...", "Zapisywanie...", "Ukládání...")
TRANSLATIONS["CPC_Status_Saved"] = T(
    "設定を保存しました。", "설정을 저장했습니다.", "设置已保存。", "Настройки сохранены.",
    "Einstellungen gespeichert.", "Paramètres enregistrés.", "Ajustes guardados.",
    "Impostazioni salvate.", "Ustawienia zapisane.", "Nastavení uloženo.")
TRANSLATIONS["CPC_Status_SaveFailed"] = T(
    "INI に書き込めませんでした。理由はログを確認してください。",
    "INI 파일에 쓸 수 없습니다. 이유는 로그를 확인하세요.",
    "无法写入 INI。原因见日志。",
    "Не удалось записать INI. Причина - в журнале.",
    "Die INI konnte nicht geschrieben werden. Warum, steht im Protokoll.",
    "Impossible d'écrire l'INI. Le journal indique pourquoi.",
    "No se pudo escribir el INI. Consulta el registro para saber por qué.",
    "Impossibile scrivere l'INI. Il motivo è nel log.",
    "Nie udało się zapisać pliku INI. Powód znajdziesz w dzienniku.",
    "Soubor INI nelze zapsat. Důvod je v protokolu.")
TRANSLATIONS["CPC_Help_Save"] = T(
    "これらのページのすべての設定をプラグインの INI に書き込み、再起動後も残るようにします。",
    "이 페이지들의 모든 설정을 플러그인 INI에 기록하여 재시작 후에도 유지되게 합니다.",
    "将这些页面上的所有设置写入插件的 INI，使其在重启后依然保留。",
    "Записывает все настройки этих страниц в INI плагина, чтобы они пережили перезапуск.",
    "Schreibt jede Einstellung dieser Seiten in die INI des Plugins, damit sie einen Neustart übersteht.",
    "Écrit tous les paramètres de ces pages dans l'INI du plugin pour qu'ils survivent à un redémarrage.",
    "Escribe todos los ajustes de estas páginas en el INI del plugin para que sobrevivan a un reinicio.",
    "Scrive tutte le impostazioni di queste pagine nell'INI del plugin, così sopravvivono a un riavvio.",
    "Zapisuje wszystkie ustawienia z tych stron do pliku INI wtyczki, aby przetrwały restart.",
    "Zapíše všechna nastavení z těchto stránek do INI pluginu, aby přežila restart.")
TRANSLATIONS["CPC_Btn_Reload"] = T(
    "INI から再読み込み", "INI에서 다시 불러오기", "从 INI 重新载入", "Перечитать INI",
    "Aus INI neu laden", "Recharger depuis l'INI", "Recargar desde el INI",
    "Ricarica dall'INI", "Wczytaj ponownie z INI", "Načíst znovu z INI")
TRANSLATIONS["CPC_Status_Reloading"] = T(
    "再読み込み中...", "다시 불러오는 중...", "正在重新载入…", "Перечитывание...",
    "Neu laden ...", "Rechargement...", "Recargando...", "Ricaricamento...", "Wczytywanie...", "Načítání...")
TRANSLATIONS["CPC_Status_Reloaded"] = T(
    "INI から設定を再読み込みしました。", "INI에서 설정을 다시 불러왔습니다.", "已从 INI 重新载入设置。",
    "Настройки перечитаны из INI.", "Einstellungen aus der INI neu geladen.",
    "Paramètres rechargés depuis l'INI.", "Ajustes recargados desde el INI.",
    "Impostazioni ricaricate dall'INI.", "Ustawienia wczytane ponownie z pliku INI.",
    "Nastavení znovu načteno z INI.")
TRANSLATIONS["CPC_Status_ReloadFailed"] = T(
    "INI を読み込めませんでした。理由はログを確認してください。",
    "INI 파일을 읽을 수 없습니다. 이유는 로그를 확인하세요.",
    "无法读取 INI。原因见日志。",
    "Не удалось прочитать INI. Причина - в журнале.",
    "Die INI konnte nicht gelesen werden. Warum, steht im Protokoll.",
    "Impossible de lire l'INI. Le journal indique pourquoi.",
    "No se pudo leer el INI. Consulta el registro para saber por qué.",
    "Impossibile leggere l'INI. Il motivo è nel log.",
    "Nie udało się odczytać pliku INI. Powód znajdziesz w dzienniku.",
    "Soubor INI nelze načíst. Důvod je v protokolu.")
TRANSLATIONS["CPC_Help_Reload"] = T(
    "前回の保存以降にここで行った変更を破棄し、ディスクから INI を読み直します。",
    "마지막 저장 이후 여기서 변경한 내용을 버리고 디스크에서 INI를 다시 읽습니다.",
    "放弃上次保存后在此所做的更改，并从磁盘重新读取 INI。",
    "Отбрасывает все изменения, сделанные здесь после последнего сохранения, и перечитывает INI с диска.",
    "Verwirft jede Änderung seit dem letzten Speichern und liest die INI erneut von der Festplatte.",
    "Annule toute modification faite ici depuis le dernier enregistrement et relit l'INI depuis le disque.",
    "Descarta cualquier cambio hecho aquí desde el último guardado y vuelve a leer el INI del disco.",
    "Scarta ogni modifica fatta qui dall'ultimo salvataggio e rilegge l'INI dal disco.",
    "Odrzuca zmiany wprowadzone tutaj od ostatniego zapisu i ponownie czyta plik INI z dysku.",
    "Zahodí každou změnu provedenou zde od posledního uložení a znovu načte INI z disku.")
TRANSLATIONS["CPC_Btn_Defaults"] = T(
    "既定値に戻す", "기본값 복원", "恢复默认值", "Восстановить значения по умолчанию",
    "Standardwerte wiederherstellen", "Restaurer les valeurs par défaut", "Restaurar valores predeterminados",
    "Ripristina i valori predefiniti", "Przywróć domyślne", "Obnovit výchozí hodnoty")
TRANSLATIONS["CPC_Status_DefaultsRestored"] = T(
    "既定値に戻しました - この環境が MOD 導入前に持っていた値です。保存を押すと確定します。",
    "기본값을 복원했습니다 - 이 설치가 모드 이전에 가지고 있던 값입니다. 저장을 눌러야 유지됩니다.",
    "已恢复默认值 - 即本次安装在使用本模组之前的数值。按保存以保留。",
    "Значения по умолчанию восстановлены - те, что были в этой установке до мода. Нажмите Сохранить, чтобы оставить их.",
    "Standardwerte wiederhergestellt - die Werte, die diese Installation vor dem Mod hatte. Zum Behalten Speichern drücken.",
    "Valeurs par défaut restaurées - celles que cette installation avait avant le mod. Appuyez sur Enregistrer pour les conserver.",
    "Valores predeterminados restaurados: los que tenía esta instalación antes del mod. Pulsa Guardar para conservarlos.",
    "Valori predefiniti ripristinati - quelli che questa installazione aveva prima della mod. Premi Salva per mantenerli.",
    "Przywrócono domyślne - wartości, które ta instalacja miała przed modem. Naciśnij Zapisz, aby je zachować.",
    "Výchozí hodnoty obnoveny - ty, které tato instalace měla před modem. Stiskni Uložit, aby zůstaly.")
TRANSLATIONS["CPC_Help_Defaults"] = T(
    "すべての設定を、この環境が MOD に触られる前に持っていた値へ戻します。保存を押すまで何も書き込まれません。",
    "모든 설정을 이 설치가 모드의 영향을 받기 전 값으로 되돌립니다. 저장을 누르기 전에는 아무것도 기록되지 않습니다.",
    "把每一项设置都还原为本次安装在被模组改动之前的数值。在按下保存之前不会写入任何内容。",
    "Возвращает каждую настройку к значению, которое эта установка имела до вмешательства мода. Ничего не записывается, пока вы не нажмёте Сохранить.",
    "Setzt jede Einstellung auf den Wert zurück, den diese Installation vor dem Mod hatte. Bis zum Speichern wird nichts geschrieben.",
    "Remet chaque paramètre à la valeur que cette installation avait avant le mod. Rien n'est écrit tant que vous n'appuyez pas sur Enregistrer.",
    "Devuelve cada ajuste al valor que tenía esta instalación antes de que el mod lo tocara. No se escribe nada hasta que pulses Guardar.",
    "Riporta ogni impostazione al valore che questa installazione aveva prima della mod. Nulla viene scritto finché non premi Salva.",
    "Przywraca każde ustawienie do wartości sprzed instalacji moda. Nic nie zostaje zapisane, dopóki nie naciśniesz Zapisz.",
    "Vrátí každé nastavení na hodnotu, kterou tato instalace měla před modem. Dokud nestiskneš Uložit, nic se nezapíše.")
TRANSLATIONS["CPC_Sec_RightNow"] = T(
    "現在の状態", "지금 상태", "当前状况", "Что это значит сейчас",
    "Was das gerade bedeutet", "Ce que cela donne actuellement", "Qué significa esto ahora",
    "Cosa significa in questo momento", "Co to teraz oznacza", "Co to znamená právě teď")
TRANSLATIONS["CPC_Btn_ApplyNow"] = T(
    "今すぐ適用", "지금 적용", "立即应用", "Применить сейчас", "Jetzt anwenden",
    "Appliquer maintenant", "Aplicar ahora", "Applica ora", "Zastosuj teraz", "Použít nyní")
TRANSLATIONS["CPC_Status_Applied"] = T(
    "適用しました。", "적용했습니다.", "已应用。", "Применено.", "Angewendet.",
    "Appliqué.", "Aplicado.", "Applicato.", "Zastosowano.", "Použito.")
TRANSLATIONS["CPC_NoCharacter"] = T(
    "キャラクターがまだ読み込まれていません。", "아직 캐릭터를 불러오지 않았습니다.", "尚未载入角色。",
    "Персонаж ещё не загружен.", "Noch kein Charakter geladen.", "Aucun personnage chargé pour l'instant.",
    "Todavía no hay ningún personaje cargado.", "Nessun personaggio caricato per ora.",
    "Nie wczytano jeszcze postaci.", "Zatím není načtena žádná postava.")
TRANSLATIONS["CPC_InertNotice"] = T(
    "このビルドでは無効: %s。以下の値は記憶され保存されるので、対応が入った日にすぐ使えます - パッチタブを参照してください。",
    "이 빌드에서는 비활성: %s. 아래 값은 기억되고 저장되므로, 적용되는 날 바로 쓸 수 있습니다 - 패치 탭을 보세요.",
    "本版本中未启用：%s。下面的数值仍会被记住并保存，等到该功能落地即可直接使用 - 参见补丁标签页。",
    "В этой сборке не работает: %s. Значения ниже запоминаются и сохраняются, поэтому настройка будет готова в день появления - см. вкладку патчей.",
    "In diesem Build nicht aktiv: %s. Die Werte unten werden gemerkt und gespeichert, sind also bereit, sobald es kommt - siehe Registerkarte Patches.",
    "Inactif dans cette version : %s. Les valeurs ci-dessous sont mémorisées et enregistrées, donc prêtes le jour où cela arrivera - voir l'onglet Patches.",
    "No activo en esta versión: %s. Los valores de abajo se recuerdan y se guardan, así que estarán listos el día que llegue - mira la pestaña Patches.",
    "Non attivo in questa build: %s. I valori sotto vengono ricordati e salvati, pronti per il giorno in cui arriverà - vedi la scheda Patches.",
    "Nieaktywne w tej wersji: %s. Wartości poniżej są zapamiętywane i zapisywane, więc będą gotowe, gdy funkcja się pojawi - zobacz kartę Patches.",
    "V tomto sestavení není aktivní: %s. Hodnoty níže se pamatují a ukládají, takže budou připravené v den, kdy to přijde - viz karta Patches.")

# --- Levelling -----------------------------------------------------------------------------------
TRANSLATIONS["CPC_Lvl_Intro"] = T(
    "キャラクターレベル 1 つ分の費用です。Skyrim は次のレベルに必要な経験値を  基本値 + (レベルごとの値 x 現在のレベル)  として計算します。",
    "캐릭터 레벨 하나의 비용입니다. Skyrim은 다음 레벨에 필요한 경험치를  기본값 + (레벨당 값 x 현재 레벨)  로 계산합니다.",
    "提升一个角色等级的花费。Skyrim 计算下一级所需经验的方式是：  基础值 + (每级值 x 当前等级)。",
    "Сколько стоит один уровень персонажа. Skyrim считает опыт до следующего уровня как:  база + (за уровень x ваш уровень).",
    "Was eine Charakterstufe kostet. Skyrim berechnet die Erfahrung bis zur nächsten Stufe als:  Basis + (pro Stufe x deine Stufe).",
    "Ce que coûte un niveau de personnage. Skyrim calcule l'expérience nécessaire au niveau suivant ainsi :  base + (par niveau x votre niveau).",
    "Lo que cuesta un nivel de personaje. Skyrim calcula la experiencia hasta el siguiente nivel así:  base + (por nivel x tu nivel).",
    "Quanto costa un livello del personaggio. Skyrim calcola l'esperienza per il livello successivo così:  base + (per livello x il tuo livello).",
    "Ile kosztuje jeden poziom postaci. Skyrim liczy doświadczenie do następnego poziomu jako:  baza + (na poziom x twój poziom).",
    "Kolik stojí jedna úroveň postavy. Skyrim počítá zkušenosti do další úrovně jako:  základ + (za úroveň x tvoje úroveň).")
TRANSLATIONS["CPC_Lvl_NotCaptured"] = T(
    "この環境自身のレベル費用設定を読み取れなかったため、このタブは何も書き込みません。ログを確認してください。",
    "이 설치의 레벨 비용 설정을 읽을 수 없어 이 탭은 아무것도 기록하지 않습니다. 로그를 확인하세요.",
    "无法读取本次安装自身的等级花费设置，因此该标签页不会写入任何内容。请查看日志。",
    "Собственные настройки стоимости уровня этой установки прочитать не удалось, поэтому вкладка ничего не пишет. Смотрите журнал.",
    "Die eigenen Stufenkosten-Einstellungen dieser Installation konnten nicht gelesen werden, deshalb schreibt diese Registerkarte nichts. Siehe Protokoll.",
    "Les paramètres de coût de niveau propres à cette installation n'ont pas pu être lus ; cet onglet n'écrira donc rien. Voir le journal.",
    "No se pudieron leer los ajustes de coste de nivel de esta instalación, así que esta pestaña no escribirá nada. Consulta el registro.",
    "Non è stato possibile leggere le impostazioni di costo del livello di questa installazione, quindi questa scheda non scriverà nulla. Vedi il log.",
    "Nie udało się odczytać własnych ustawień kosztu poziomu tej instalacji, więc ta karta niczego nie zapisze. Zobacz dziennik.",
    "Vlastní nastavení ceny úrovně této instalace se nepodařilo načíst, takže tato karta nic nezapíše. Viz protokol.")
TRANSLATIONS["CPC_Lvl_SecCost"] = T(
    "レベルの費用", "레벨 비용", "等级花费", "Стоимость уровня", "Kosten einer Stufe",
    "Coût d'un niveau", "Coste de un nivel", "Costo di un livello", "Koszt poziomu", "Cena úrovně")
TRANSLATIONS["CPC_Lvl_Control"] = T(
    "レベルの費用を制御する", "레벨 비용 제어", "控制等级花费", "Управлять стоимостью уровня",
    "Kosten einer Stufe steuern", "Contrôler le coût d'un niveau", "Controlar el coste de un nivel",
    "Controlla il costo di un livello", "Kontroluj koszt poziomu", "Řídit cenu úrovně")
TRANSLATIONS["CPC_Lvl_HelpControl"] = T(
    "既定はオフで、オフの間この MOD は何も書き込みません - ゲームのレベル進行は以前とまったく同じで、これらの値を設定する他の MOD もそのまま保たれます。オンにすると下の 2 つの値がセーブ読み込みのたびに適用され続けます。",
    "기본값은 꺼짐이며, 꺼져 있는 동안 이 모드는 아무것도 기록하지 않습니다 - 게임의 레벨 진행은 이전과 똑같고, 이 값을 설정하는 다른 모드도 그대로 유지됩니다. 켜면 아래 두 값이 세이브를 불러올 때마다 다시 적용됩니다.",
    "默认关闭；关闭时本模组不写入任何内容 - 游戏升级方式与之前完全相同，设置这些数值的其他模组也会保持不变。开启后，下面两个数值会在每次读取存档时被应用并重新应用。",
    "По умолчанию выключено, и пока выключено, мод ничего не пишет - игра растит уровни ровно как раньше, а любой другой мод, задающий эти значения, сохраняет их. Включите - и два значения ниже применяются заново при каждой загрузке сохранения.",
    "Standardmäßig aus, und solange es aus ist, schreibt dieser Mod nichts - dein Spiel steigt genau wie zuvor auf, und jeder andere Mod, der diese Werte setzt, behält sie. Eingeschaltet werden die beiden Werte unten bei jedem Laden eines Spielstands erneut angewendet.",
    "Désactivé par défaut ; tant que c'est désactivé, ce mod n'écrit rien - votre jeu monte en niveau exactement comme avant, et tout autre mod qui règle ces valeurs les conserve. Une fois activé, les deux valeurs ci-dessous sont appliquées et réappliquées à chaque chargement de sauvegarde.",
    "Desactivado por defecto; mientras lo esté, este mod no escribe nada: tu juego sube de nivel exactamente igual que antes y cualquier otro mod que fije estos valores los conserva. Al activarlo, los dos valores de abajo se aplican y se reaplican cada vez que se carga una partida.",
    "Disattivato per impostazione predefinita e, finché è disattivato, questa mod non scrive nulla - il gioco sale di livello esattamente come prima e ogni altra mod che imposta questi valori li mantiene. Attivandolo, i due valori sotto vengono applicati e riapplicati a ogni caricamento di un salvataggio.",
    "Domyślnie wyłączone, a gdy jest wyłączone, ten mod niczego nie zapisuje - gra awansuje dokładnie tak jak wcześniej, a każdy inny mod ustawiający te wartości je zachowuje. Po włączeniu obie wartości poniżej są stosowane przy każdym wczytaniu zapisu.",
    "Ve výchozím stavu vypnuto a dokud je vypnuto, tento mod nic nezapisuje - hra postupuje v úrovních přesně jako dřív a každý jiný mod, který tyto hodnoty nastavuje, si je ponechá. Po zapnutí se obě hodnoty níže použijí znovu při každém načtení pozice.")
TRANSLATIONS["CPC_Lvl_Base"] = T(
    "基本費用", "기본 비용", "基础花费", "Базовая стоимость", "Grundkosten",
    "Coût de base", "Coste base", "Costo base", "Koszt bazowy", "Základní cena")
TRANSLATIONS["CPC_Lvl_HelpBase"] = T(
    "レベルごとに必ず支払う固定部分です。この環境自身の値は下に表示されます。",
    "레벨마다 항상 지불하는 고정 부분입니다. 이 설치의 값은 아래에 표시됩니다.",
    "每一级都要支付的固定部分。本次安装自身的数值显示在下方。",
    "Постоянная часть стоимости, платится на каждом уровне. Собственное значение этой установки показано ниже.",
    "Der feste Teil der Kosten, der auf jeder Stufe anfällt. Der eigene Wert dieser Installation steht unten.",
    "La part fixe du coût, payée à chaque niveau. La valeur propre à cette installation est indiquée ci-dessous.",
    "La parte fija del coste, que se paga en cada nivel. El valor propio de esta instalación se muestra abajo.",
    "La parte fissa del costo, pagata a ogni livello. Il valore proprio di questa installazione è mostrato sotto.",
    "Stała część kosztu, płacona na każdym poziomie. Własna wartość tej instalacji jest pokazana poniżej.",
    "Pevná část ceny, placená na každé úrovni. Vlastní hodnota této instalace je uvedena níže.")
TRANSLATIONS["CPC_Lvl_Mult"] = T(
    "レベルごと", "레벨당", "每级", "За уровень", "Pro Stufe",
    "Par niveau", "Por nivel", "Per livello", "Na poziom", "Za úroveň")
TRANSLATIONS["CPC_Lvl_HelpMult"] = T(
    "すでに持っているレベル 1 つごとに費用へ加算されます。上げると後半のレベルほど遅くなり、下げると曲線が平坦になります。",
    "이미 가진 레벨 하나마다 비용에 더해집니다. 올리면 후반 레벨이 점점 느려지고, 내리면 곡선이 완만해집니다.",
    "按你已有的每一级追加到花费上。调高会让后期等级越来越慢，调低则使曲线更平缓。",
    "Добавляется к стоимости за каждый уже имеющийся уровень. Больше - поздние уровни всё медленнее; меньше - кривая ровнее.",
    "Wird für jede bereits erreichte Stufe zu den Kosten addiert. Höher macht spätere Stufen zunehmend langsamer, niedriger flacht die Kurve ab.",
    "S'ajoute au coût pour chaque niveau déjà atteint. Augmenter ralentit progressivement les niveaux suivants ; diminuer aplatit la courbe.",
    "Se suma al coste por cada nivel que ya tengas. Subirlo hace que los niveles tardíos sean cada vez más lentos; bajarlo aplana la curva.",
    "Si aggiunge al costo per ogni livello già raggiunto. Alzarlo rende i livelli successivi sempre più lenti; abbassarlo appiattisce la curva.",
    "Doliczane do kosztu za każdy już posiadany poziom. Wyższa wartość spowalnia późniejsze poziomy, niższa spłaszcza krzywą.",
    "Přičítá se k ceně za každou již dosaženou úroveň. Vyšší hodnota postupně zpomaluje pozdější úrovně, nižší křivku zplošťuje.")
TRANSLATIONS["CPC_Lvl_CostLine"] = T(
    "レベル %u から %u までの費用は経験値 %.0f  (%.0f + %.1f x %u)",
    "레벨 %u에서 %u까지의 비용은 경험치 %.0f  (%.0f + %.1f x %u)",
    "从等级 %u 到 %u 需要经验 %.0f  (%.0f + %.1f x %u)",
    "С уровня %u до %u нужно %.0f опыта  (%.0f + %.1f x %u)",
    "Von Stufe %u auf %u kostet %.0f Erfahrung  (%.0f + %.1f x %u)",
    "Du niveau %u au niveau %u : %.0f d'expérience  (%.0f + %.1f x %u)",
    "Del nivel %u al %u cuesta %.0f de experiencia  (%.0f + %.1f x %u)",
    "Dal livello %u al %u costa %.0f di esperienza  (%.0f + %.1f x %u)",
    "Z poziomu %u na %u kosztuje %.0f doświadczenia  (%.0f + %.1f x %u)",
    "Z úrovně %u na %u stojí %.0f zkušeností  (%.0f + %.1f x %u)")
TRANSLATIONS["CPC_Lvl_VanillaLine"] = T(
    "この環境自身の値: 基本 %.0f、レベルごと %.1f",
    "이 설치의 값: 기본 %.0f, 레벨당 %.1f",
    "本次安装自身的数值：基础 %.0f，每级 %.1f",
    "Собственные значения этой установки: база %.0f, за уровень %.1f",
    "Eigene Werte dieser Installation: %.0f Basis, %.1f pro Stufe",
    "Valeurs propres à cette installation : %.0f de base, %.1f par niveau",
    "Valores propios de esta instalación: %.0f de base, %.1f por nivel",
    "Valori propri di questa installazione: %.0f di base, %.1f per livello",
    "Własne wartości tej instalacji: baza %.0f, na poziom %.1f",
    "Vlastní hodnoty této instalace: základ %.0f, za úroveň %.1f")
TRANSLATIONS["CPC_Lvl_NotControlling"] = T(
    "費用を制御していません - 上の値はゲームが実際に使っているものです。",
    "비용을 제어하고 있지 않습니다 - 위 값은 게임이 실제로 쓰는 값입니다.",
    "未控制花费 - 上面的数值就是游戏当前实际使用的。",
    "Стоимость не управляется - значения выше те, что использует игра.",
    "Die Kosten werden nicht gesteuert - die Werte oben sind die, die das Spiel verwendet.",
    "Le coût n'est pas contrôlé - les valeurs ci-dessus sont celles qu'utilise le jeu.",
    "No se está controlando el coste: los valores de arriba son los que usa el juego.",
    "Il costo non è controllato - i valori sopra sono quelli che il gioco sta usando.",
    "Koszt nie jest kontrolowany - powyższe wartości to te, których używa gra.",
    "Cena není řízena - hodnoty výše jsou ty, které hra používá.")
TRANSLATIONS["CPC_Lvl_HelpApply"] = T(
    "値を書き直します。セーブ読み込み時、ニューゲーム開始時、上の項目を変更したときにも自動で実行されます - 常駐処理はありません。",
    "값을 다시 씁니다. 세이브를 불러올 때, 새 게임을 시작할 때, 위 항목을 바꿀 때도 자동으로 실행됩니다 - 백그라운드 처리는 없습니다.",
    "重新写入这些数值。读取存档、开始新游戏以及你更改上面任何内容时也会自动执行 - 没有后台轮询。",
    "Перезаписывает значения. Также срабатывает само при загрузке сохранения, при новой игре и при изменении чего-либо выше - ничего не работает в фоне.",
    "Schreibt die Werte neu. Läuft außerdem von selbst beim Laden eines Spielstands, bei einem neuen Spiel und wenn du oben etwas änderst - nichts läuft im Hintergrund.",
    "Réécrit les valeurs. S'exécute aussi tout seul au chargement d'une sauvegarde, au lancement d'une nouvelle partie et quand vous modifiez quelque chose ci-dessus - rien ne tourne en arrière-plan.",
    "Reescribe los valores. También se ejecuta solo al cargar una partida, al empezar una nueva y cuando cambias algo arriba: nada corre en segundo plano.",
    "Riscrive i valori. Viene eseguito anche da solo al caricamento di un salvataggio, all'inizio di una nuova partita e quando modifichi qualcosa sopra - nulla gira in background.",
    "Ponownie zapisuje wartości. Uruchamia się też samo przy wczytaniu zapisu, przy nowej grze i gdy zmienisz cokolwiek powyżej - nic nie działa w tle.",
    "Znovu zapíše hodnoty. Spustí se také samo při načtení pozice, při nové hře a když nahoře něco změníš - nic neběží na pozadí.")

# --- Skills --------------------------------------------------------------------------------------
TRANSLATIONS["CPC_Skl_Intro"] = T(
    "各スキルがどこで止まるか、そしてゲーム自身の計算式がそのスキルをいくつとして読むか - スキル表示が 300 でも戦闘計算では 100 として扱う、といったことができます。",
    "각 기술이 어디서 멈추는지, 그리고 게임 자체 계산식이 그 기술을 얼마로 읽는지 - 기술이 300으로 보여도 전투 계산은 100으로 취급할 수 있습니다.",
    "每项技能在何处停止成长，以及游戏自身的公式把它读作多少 - 技能可以显示 300，而战斗计算仍按 100 处理。",
    "Где останавливается каждый навык и какое значение читают собственные формулы игры - навык может показывать 300, пока боевая математика считает его равным 100.",
    "Wo jede Fertigkeit stehen bleibt und welchen Wert die spieleigenen Formeln dafür lesen - eine Fertigkeit kann 300 anzeigen, während die Kampfrechnung sie weiter als 100 behandelt.",
    "Où chaque compétence s'arrête, et ce que les formules du jeu lisent pour elle - une compétence peut afficher 300 tandis que les calculs de combat la traitent encore comme 100.",
    "Dónde se detiene cada habilidad y qué valor leen las fórmulas del propio juego: una habilidad puede mostrar 300 mientras los cálculos de combate la siguen tratando como 100.",
    "Dove si ferma ogni abilità e quale valore leggono le formule del gioco - un'abilità può mostrare 300 mentre i calcoli di combattimento la trattano ancora come 100.",
    "Gdzie zatrzymuje się każda umiejętność i jaką wartość czytają własne wzory gry - umiejętność może pokazywać 300, podczas gdy matematyka walki traktuje ją jak 100.",
    "Kde se každá dovednost zastaví a jakou hodnotu pro ni čtou vlastní vzorce hry - dovednost může ukazovat 300, zatímco bojové výpočty ji stále berou jako 100.")
TRANSLATIONS["CPC_Skl_CapsInert"] = T(
    "このビルドではスキル上限パッチが有効ではないため、すべてのスキルはバニラどおり 100 で止まります。以下の値は記憶され保存されますが、まだ何もしません。パッチタブを参照してください。",
    "이 빌드에서는 기술 상한 패치가 활성화되어 있지 않아 모든 기술은 바닐라와 똑같이 100에서 멈춥니다. 아래 값은 기억되고 저장되지만 아직 아무 일도 하지 않습니다. 패치 탭을 보세요.",
    "本版本未启用技能上限补丁，因此所有技能仍与原版一样止步于 100。下面的数值会被记住并保存，但目前不起作用。请参见补丁标签页。",
    "В этой сборке патч потолков навыков НЕ активен, поэтому каждый навык по-прежнему останавливается на 100, как в оригинале. Значения ниже запоминаются и сохраняются, но пока ничего не делают. См. вкладку патчей.",
    "Der Fertigkeitsgrenzen-Patch ist in diesem Build NICHT aktiv, jede Fertigkeit endet also weiterhin genau wie im Original bei 100. Die Werte unten werden gemerkt und gespeichert, tun aber noch nichts. Siehe Registerkarte Patches.",
    "Le correctif de plafond de compétence n'est PAS actif dans cette version : chaque compétence s'arrête donc à 100 comme dans le jeu de base. Les valeurs ci-dessous sont mémorisées et enregistrées mais ne font encore rien. Voir l'onglet Patches.",
    "El parche de límite de habilidad NO está activo en esta versión, así que todas las habilidades siguen deteniéndose en 100 igual que en el juego base. Los valores de abajo se recuerdan y se guardan, pero todavía no hacen nada. Mira la pestaña Patches.",
    "La patch dei limiti di abilità NON è attiva in questa build, quindi ogni abilità si ferma ancora a 100 esattamente come nel gioco base. I valori sotto vengono ricordati e salvati, ma per ora non fanno nulla. Vedi la scheda Patches.",
    "Łatka limitów umiejętności NIE jest aktywna w tej wersji, więc każda umiejętność wciąż zatrzymuje się na 100, jak w podstawowej grze. Wartości poniżej są zapamiętywane i zapisywane, ale na razie nic nie robią. Zobacz kartę Patches.",
    "Záplata stropů dovedností v tomto sestavení NENÍ aktivní, takže každá dovednost se stále zastaví na 100 jako v základní hře. Hodnoty níže se pamatují a ukládají, ale zatím nic nedělají. Viz karta Patches.")
TRANSLATIONS["CPC_Skl_ControlCaps"] = T(
    "スキル上限を制御する", "기술 상한 제어", "控制技能上限", "Управлять потолками навыков",
    "Fertigkeitsgrenzen steuern", "Contrôler les plafonds de compétence", "Controlar los límites de habilidad",
    "Controlla i limiti di abilità", "Kontroluj limity umiejętności", "Řídit stropy dovedností")
TRANSLATIONS["CPC_Skl_HelpCaps"] = T(
    "既定はオフです。オフの間、この MOD はスキルについて何も主張しません - オフのままなら、ゲーム内の命令は 1 つも書き換えられません。",
    "기본값은 꺼짐입니다. 꺼져 있는 동안 이 모드는 기술에 대해 아무것도 주장하지 않습니다 - 꺼져 있으면 게임의 명령어는 단 하나도 수정되지 않습니다.",
    "默认关闭。关闭时本模组对你的技能不做任何干预 - 只要关着，游戏中不会有一条指令被改写。",
    "По умолчанию выключено. Пока выключено, мод ничего не утверждает о ваших навыках - при выключенном состоянии в игре не изменяется ни одна инструкция.",
    "Standardmäßig aus. Solange es aus ist, behauptet dieser Mod nichts über deine Fertigkeiten - ausgeschaltet wird keine einzige Instruktion im Spiel verändert.",
    "Désactivé par défaut. Tant que c'est désactivé, ce mod n'impose rien à vos compétences - dans cet état, pas une seule instruction du jeu n'est modifiée.",
    "Desactivado por defecto. Mientras lo esté, este mod no impone nada sobre tus habilidades: apagado, no se modifica ni una sola instrucción del juego.",
    "Disattivato per impostazione predefinita. Finché è disattivato, questa mod non impone nulla sulle tue abilità - da spento non viene modificata nemmeno un'istruzione del gioco.",
    "Domyślnie wyłączone. Gdy jest wyłączone, mod niczego nie narzuca twoim umiejętnościom - przy wyłączonej opcji nie jest zmieniana ani jedna instrukcja gry.",
    "Ve výchozím stavu vypnuto. Dokud je vypnuto, mod o tvých dovednostech nic netvrdí - ve vypnutém stavu se v hře nezmění jediná instrukce.")
TRANSLATIONS["CPC_Skl_ControlRates"] = T(
    "スキル経験値の倍率を制御する", "기술 경험치 배율 제어", "控制技能经验倍率",
    "Управлять скоростью опыта навыков", "Fertigkeitserfahrungsraten steuern",
    "Contrôler les taux d'expérience des compétences", "Controlar las tasas de experiencia de habilidad",
    "Controlla i tassi di esperienza delle abilità", "Kontroluj tempo doświadczenia umiejętności",
    "Řídit rychlost zkušeností dovedností")
TRANSLATIONS["CPC_Skl_HelpRates"] = T(
    "既定はオフです。下のスキルごとの経験値倍率を有効にします。",
    "기본값은 꺼짐입니다. 아래의 기술별 경험치 배율을 켭니다.",
    "默认关闭。开启下面的各技能经验倍率。",
    "По умолчанию выключено. Включает множители опыта по каждому навыку ниже.",
    "Standardmäßig aus. Schaltet die Erfahrungsmultiplikatoren je Fertigkeit unten ein.",
    "Désactivé par défaut. Active les multiplicateurs d'expérience par compétence ci-dessous.",
    "Desactivado por defecto. Activa los multiplicadores de experiencia por habilidad de abajo.",
    "Disattivato per impostazione predefinita. Attiva i moltiplicatori di esperienza per abilità qui sotto.",
    "Domyślnie wyłączone. Włącza mnożniki doświadczenia dla poszczególnych umiejętności poniżej.",
    "Ve výchozím stavu vypnuto. Zapíná násobitele zkušeností pro jednotlivé dovednosti níže.")
TRANSLATIONS["CPC_Skl_ToLevelUnavailable"] = T(
    "スキル上昇 -> レベル: 利用できません", "기술 상승 -> 레벨: 사용할 수 없음", "技能提升 -> 等级：不可用",
    "Рост навыка -> уровень: недоступно", "Fertigkeitsanstieg -> Stufe: nicht verfügbar",
    "Progression de compétence -> niveau : indisponible", "Subida de habilidad -> nivel: no disponible",
    "Aumento abilità -> livello: non disponibile", "Wzrost umiejętności -> poziom: niedostępne",
    "Zvýšení dovednosti -> úroveň: nedostupné")
TRANSLATIONS["CPC_Skl_ExperienceOwns"] = T(
    "Experience が導入されていて、キャラクター経験値の入手元を管理しています。そのためこの倍率は何もしません。Levelling タブのレベル費用には影響しません - Experience 自身のページも、それらの設定を編集する MOD とは互換であり、推奨すると書いています。",
    "Experience가 설치되어 캐릭터 경험치의 출처를 관리합니다. 따라서 이 배율은 아무 일도 하지 않습니다. Levelling 탭의 레벨 비용은 영향을 받지 않습니다 - Experience 자체 페이지도 그 설정을 수정하는 모드와 호환되며 권장한다고 밝히고 있습니다.",
    "已安装 Experience，它掌管角色经验的来源，因此该倍率不起作用。Levelling 标签页上的等级花费不受影响 - Experience 自己的页面也说明修改那些设置的模组是兼容的，并加以推荐。",
    "Установлен Experience, и он владеет тем, откуда берётся опыт персонажа, поэтому этот множитель ничего не даст. СТОИМОСТЬ уровня на вкладке Levelling это не затрагивает - собственная страница Experience говорит, что моды, меняющие эти настройки, совместимы, и рекомендует их.",
    "Experience ist installiert und bestimmt, woher Charaktererfahrung kommt, daher würde dieser Multiplikator nichts bewirken. Die Stufen-KOSTEN auf der Registerkarte Levelling bleiben davon unberührt - die Seite von Experience selbst nennt Mods, die diese Einstellungen ändern, kompatibel und empfiehlt sie.",
    "Experience est installé et détermine d'où vient l'expérience du personnage ; ce multiplicateur ne ferait donc rien. Le COÛT de niveau de l'onglet Levelling n'est pas affecté - la page d'Experience indique elle-même que les mods modifiant ces paramètres sont compatibles et les recommande.",
    "Experience está instalado y controla de dónde viene la experiencia del personaje, así que este multiplicador no haría nada. El COSTE de nivel de la pestaña Levelling no se ve afectado: la propia página de Experience dice que los mods que editan esos ajustes son compatibles y los recomienda.",
    "Experience è installata e governa da dove arriva l'esperienza del personaggio, quindi questo moltiplicatore non farebbe nulla. Il COSTO del livello nella scheda Levelling non è toccato - la pagina di Experience stessa dice che le mod che modificano quelle impostazioni sono compatibili e le consiglia.",
    "Zainstalowano Experience i to on decyduje, skąd bierze się doświadczenie postaci, więc ten mnożnik nic by nie dał. KOSZT poziomu na karcie Levelling pozostaje nietknięty - strona samego Experience mówi, że mody edytujące te ustawienia są zgodne, i je poleca.",
    "Je nainstalován Experience a ten určuje, odkud se bere zkušenost postavy, takže tento násobitel by nic neudělal. CENA úrovně na kartě Levelling tím není dotčena - vlastní stránka Experience uvádí, že mody upravující tato nastavení jsou kompatibilní, a doporučuje je.")
TRANSLATIONS["CPC_Skl_ToLevel"] = T(
    "スキル上昇 -> レベル", "기술 상승 -> 레벨", "技能提升 -> 等级", "Рост навыка -> уровень",
    "Fertigkeitsanstieg -> Stufe", "Progression de compétence -> niveau", "Subida de habilidad -> nivel",
    "Aumento abilità -> livello", "Wzrost umiejętności -> poziom", "Zvýšení dovednosti -> úroveň")
TRANSLATIONS["CPC_Skl_HelpToLevel"] = T(
    "スキル上昇がキャラクターレベルへどれだけ寄与するかを倍率で変えます。これは Levelling タブのレベル費用の裏側です: 上げれば、レベル費用を変えずにレベルアップが早くなります。",
    "기술 상승이 캐릭터 레벨에 얼마나 기여하는지를 배율로 바꿉니다. 이는 Levelling 탭의 레벨 비용과 반대편에 있는 값입니다: 올리면 레벨 비용을 바꾸지 않고도 레벨업이 빨라집니다.",
    "调整一次技能提升对角色等级的贡献。这是 Levelling 标签页等级花费的另一面：调高就能在不改变升级花费的情况下更快升级。",
    "Умножает то, сколько рост навыка платит в ваш УРОВЕНЬ персонажа. Это обратная сторона стоимости уровня на вкладке Levelling: поднимите - и уровни идут быстрее без изменения их стоимости.",
    "Multipliziert, was ein Fertigkeitsanstieg zu deiner CHARAKTERSTUFE beiträgt. Das ist die andere Seite der Stufenkosten auf der Registerkarte Levelling: höher gesetzt steigst du schneller auf, ohne die Kosten zu ändern.",
    "Multiplie ce qu'une progression de compétence rapporte à votre niveau de PERSONNAGE. C'est l'autre face du coût de niveau de l'onglet Levelling : l'augmenter fait monter les niveaux plus vite sans changer leur coût.",
    "Multiplica lo que una subida de habilidad aporta a tu nivel de PERSONAJE. Es la otra cara del coste de nivel de la pestaña Levelling: súbelo y los niveles llegan antes sin cambiar lo que cuestan.",
    "Moltiplica quanto un aumento di abilità paga verso il tuo livello del PERSONAGGIO. È l'altra faccia del costo del livello nella scheda Levelling: alzalo e i livelli arrivano più in fretta senza cambiarne il costo.",
    "Mnoży to, ile wzrost umiejętności wnosi do twojego poziomu POSTACI. To druga strona kosztu poziomu z karty Levelling: podnieś go, a poziomy przychodzą szybciej bez zmiany ich kosztu.",
    "Násobí to, kolik zvýšení dovednosti přispěje k tvé ÚROVNI postavy. Je to druhá strana ceny úrovně na kartě Levelling: zvyš ho a úrovně přicházejí rychleji, aniž by se změnila jejich cena.")
TRANSLATIONS["CPC_Skl_SecNow"] = T(
    "現在のスキル", "지금의 기술", "你当前的技能", "Ваши навыки сейчас", "Deine Fertigkeiten gerade",
    "Vos compétences actuelles", "Tus habilidades ahora", "Le tue abilità in questo momento",
    "Twoje umiejętności teraz", "Tvoje dovednosti právě teď")
TRANSLATIONS["CPC_Skl_NoCharacter"] = T(
    "キャラクターが読み込まれていません - スキルを見るにはセーブを読み込んでください。",
    "캐릭터가 없습니다 - 기술을 보려면 세이브를 불러오세요.",
    "未载入角色 - 读取存档后才能看到技能。",
    "Персонаж не загружен - загрузите сохранение, чтобы увидеть навыки.",
    "Kein Charakter geladen - lade einen Spielstand, um deine Fertigkeiten zu sehen.",
    "Aucun personnage chargé - chargez une sauvegarde pour voir vos compétences.",
    "No hay personaje cargado: carga una partida para ver tus habilidades.",
    "Nessun personaggio caricato - carica un salvataggio per vedere le tue abilità.",
    "Nie wczytano postaci - wczytaj zapis, aby zobaczyć umiejętności.",
    "Není načtena postava - načti pozici, abys viděl své dovednosti.")
TRANSLATIONS["CPC_Skl_CharacterLine"] = T(
    "キャラクターレベル %u - 次のレベルまで %.0f / %.0f 経験値",
    "캐릭터 레벨 %u - 다음 레벨까지 경험치 %.0f / %.0f",
    "角色等级 %u - 距下一级 %.0f / %.0f 经验",
    "Уровень персонажа %u - %.0f из %.0f опыта до следующего уровня",
    "Charakterstufe %u - %.0f von %.0f Erfahrung bis zur nächsten Stufe",
    "Niveau de personnage %u - %.0f sur %.0f d'expérience vers le niveau suivant",
    "Nivel de personaje %u: %.0f de %.0f de experiencia hacia el siguiente nivel",
    "Livello personaggio %u - %.0f di %.0f di esperienza verso il livello successivo",
    "Poziom postaci %u - %.0f z %.0f doświadczenia do następnego poziomu",
    "Úroveň postavy %u - %.0f z %.0f zkušeností do další úrovně")
TRANSLATIONS["CPC_Skl_Row"] = T(
    "%-12s  %5.1f   %.0f / %.0f", "%-12s  %5.1f   %.0f / %.0f", "%-12s  %5.1f   %.0f / %.0f",
    "%-12s  %5.1f   %.0f / %.0f", "%-12s  %5.1f   %.0f / %.0f", "%-12s  %5.1f   %.0f / %.0f",
    "%-12s  %5.1f   %.0f / %.0f", "%-12s  %5.1f   %.0f / %.0f", "%-12s  %5.1f   %.0f / %.0f",
    "%-12s  %5.1f   %.0f / %.0f")
TRANSLATIONS["CPC_Skl_Cap"] = T(
    "上限", "상한", "上限", "Потолок", "Grenze", "Plafond", "Límite", "Limite", "Limit", "Strop")
TRANSLATIONS["CPC_Skl_HelpCap"] = T(
    "このスキルが成長を止めるレベルです。100 がバニラです。",
    "이 기술이 성장을 멈추는 레벨입니다. 100이 바닐라입니다.",
    "该技能停止成长的等级。100 为原版数值。",
    "Уровень, на котором этот навык перестаёт расти. 100 - как в оригинале.",
    "Die Stufe, ab der diese Fertigkeit nicht weiter steigt. 100 ist Original.",
    "Le niveau où cette compétence cesse de progresser. 100 correspond au jeu de base.",
    "El nivel en el que esta habilidad deja de subir. 100 es el valor original.",
    "Il livello a cui questa abilità smette di crescere. 100 è il valore base.",
    "Poziom, na którym ta umiejętność przestaje rosnąć. 100 to wartość podstawowa.",
    "Úroveň, na které tato dovednost přestane růst. 100 je základní hodnota.")
TRANSLATIONS["CPC_Skl_FormulaCap"] = T(
    "計算式上の上限", "계산식 상한", "公式上限", "Потолок для формул", "Formelgrenze",
    "Plafond de formule", "Límite de fórmula", "Limite di formula", "Limit we wzorach", "Strop pro vzorce")
TRANSLATIONS["CPC_Skl_HelpFormulaCap"] = T(
    "スキル自身の表示がいくつであれ、ゲーム自身の計算がこのスキルに対して使う値です。これを 100 のままにすると、スキルの数値が伸び続けても戦闘や価格の均衡は保たれます。",
    "기술 자체가 얼마로 표시되든, 게임의 계산이 이 기술에 대해 사용하는 값입니다. 100으로 두면 기술 수치가 계속 올라가도 전투와 가격 균형이 유지됩니다.",
    "无论技能本身显示多少，游戏自身的计算都会按这个数值来读取该技能。保持 100 可以在技能数字不断攀升时维持战斗与价格的平衡。",
    "Значение, которое собственные расчёты игры используют для этого навыка, каким бы высоким ни было само число. Оставив 100, вы сохраняете баланс боя и цен, пока число навыка растёт.",
    "Der Wert, den die spieleigenen Berechnungen für diese Fertigkeit verwenden, egal wie hoch die Fertigkeit selbst steht. Bei 100 bleiben Kampf und Preise ausgewogen, während die Zahl weiter klettert.",
    "La valeur que les calculs du jeu utilisent pour cette compétence, quel que soit le chiffre affiché. Laisser 100 garde le combat et les prix équilibrés pendant que le nombre continue de grimper.",
    "El valor que usan los cálculos del propio juego para esta habilidad, por alto que llegue el número mostrado. Dejarlo en 100 mantiene el combate y los precios equilibrados mientras la cifra sigue subiendo.",
    "Il valore che i calcoli del gioco usano per questa abilità, per quanto in alto arrivi il numero mostrato. Lasciarlo a 100 mantiene combattimento e prezzi equilibrati mentre il numero continua a salire.",
    "Wartość, której własne obliczenia gry używają dla tej umiejętności, niezależnie od tego, jak wysoko sięga sam wskaźnik. Pozostawienie 100 utrzymuje równowagę walki i cen, gdy liczba dalej rośnie.",
    "Hodnota, kterou pro tuto dovednost používají vlastní výpočty hry, ať už dovednost sama ukazuje cokoli. Ponechání na 100 udrží boj a ceny vyvážené, zatímco číslo dál stoupá.")
TRANSLATIONS["CPC_Skl_Rate"] = T(
    "経験値倍率", "경험치 배율", "经验倍率", "Скорость опыта", "Erfahrungsrate",
    "Taux d'expérience", "Tasa de experiencia", "Tasso di esperienza", "Tempo doświadczenia", "Rychlost zkušeností")
TRANSLATIONS["CPC_Skl_HelpRate"] = T(
    "このスキルを 1 回使ったときの獲得量を倍率で変えます。1.00 がバニラで、1 未満は遅く、1 超は速くなります。",
    "이 기술을 한 번 사용했을 때의 획득량을 배율로 바꿉니다. 1.00이 바닐라이고, 1 미만은 느리며 1 초과는 빠릅니다.",
    "调整使用该技能一次所获得的量。1.00 为原版；低于 1 更慢，高于 1 更快。",
    "Умножает то, что даёт одно использование этого навыка. 1.00 - как в оригинале; меньше 1 медленнее, больше 1 быстрее.",
    "Multipliziert, was eine Anwendung dieser Fertigkeit einbringt. 1,00 ist Original; unter 1 ist langsamer, über 1 schneller.",
    "Multiplie ce que rapporte une utilisation de cette compétence. 1,00 correspond au jeu de base ; en dessous de 1 c'est plus lent, au-dessus plus rapide.",
    "Multiplica lo que aporta un uso de esta habilidad. 1,00 es el valor original; por debajo de 1 es más lento y por encima más rápido.",
    "Moltiplica quanto rende un uso di questa abilità. 1,00 è il valore base; sotto 1 è più lento, sopra 1 più veloce.",
    "Mnoży to, co daje jedno użycie tej umiejętności. 1,00 to wartość podstawowa; poniżej 1 wolniej, powyżej szybciej.",
    "Násobí to, co přinese jedno použití této dovednosti. 1,00 je základní hodnota; pod 1 je to pomalejší, nad 1 rychlejší.")

# --- Level Up ------------------------------------------------------------------------------------
TRANSLATIONS["CPC_LvlUp_Intro"] = T(
    "レベルアップで得られるもの: パークポイントと、選択に伴う体力・マジカ・スタミナ・所持重量です。",
    "레벨업으로 얻는 것: 특전 점수와, 선택에 따라 오르는 체력·매지카·스태미나·소지 중량입니다.",
    "升级会给你什么：额外的技能点，以及随你的选择而来的生命、法力、体力和负重。",
    "Что даёт повышение уровня: очки способностей и здоровье, магия, запас сил и переносимый вес, идущие вместе с вашим выбором.",
    "Was ein Stufenaufstieg bringt: die Vorteilspunkte sowie Gesundheit, Magicka, Ausdauer und Tragkraft, die mit deiner Wahl kommen.",
    "Ce qu'un passage de niveau vous donne : les points d'aptitude, ainsi que la santé, la magie, la vigueur et la charge liées à votre choix.",
    "Lo que da subir de nivel: los puntos de habilidad especial y la salud, magia, aguante y capacidad de carga que acompañan a tu elección.",
    "Cosa dà un passaggio di livello: i punti privilegio e la salute, magicka, vigore e capacità di carico che accompagnano la tua scelta.",
    "Co daje awans na poziom: punkty atutów oraz zdrowie, magia, kondycja i udźwig związane z twoim wyborem.",
    "Co dá postup na úroveň: body vlastností a zdraví, magicka, výdrž a nosnost, které přicházejí s tvou volbou.")
TRANSLATIONS["CPC_LvlUp_Inert"] = T(
    "レベルアップで得られる体力/マジカ/スタミナと所持重量はバニラのままです。パーク表は別扱いです",
    "레벨업에서 얻는 체력/매지카/스태미나와 소지 중량은 바닐라 그대로입니다. 특전 표는 별개입니다",
    "升级给予的生命/法力/体力与负重仍与原版一致；技能点表格是另外一回事",
    "здоровье/магия/запас сил и переносимый вес за уровень остаются оригинальными; таблица очков способностей - отдельно",
    "die Gesundheit/Magicka/Ausdauer und die Tragkraft eines Stufenaufstiegs sind weiterhin original; die Vorteilstabelle ist davon getrennt",
    "la santé/magie/vigueur et la charge accordées par un niveau restent celles du jeu de base ; la table d'aptitudes est distincte",
    "la salud/magia/aguante y la carga que otorga un nivel siguen siendo las originales; la tabla de puntos de habilidad va aparte",
    "la salute/magicka/vigore e il carico dati da un livello restano quelli base; la tabella dei privilegi è a parte",
    "zdrowie/magia/kondycja i udźwig przyznawane za poziom pozostają podstawowe; tabela atutów jest osobno",
    "zdraví/magicka/výdrž a nosnost dané postupem zůstávají základní; tabulka vlastností je zvlášť")
TRANSLATIONS["CPC_LvlUp_Control"] = T(
    "レベルアップの報酬を制御する", "레벨업 보상 제어", "控制升级奖励", "Управлять наградой за уровень",
    "Belohnung eines Stufenaufstiegs steuern", "Contrôler ce qu'accorde un niveau", "Controlar lo que da subir de nivel",
    "Controlla cosa concede un passaggio di livello", "Kontroluj nagrody za awans", "Řídit odměny za postup na úroveň")
TRANSLATIONS["CPC_LvlUp_HelpControl"] = T(
    "既定はオフです。オフの間、この MOD はレベルアップ報酬について何も主張しません。",
    "기본값은 꺼짐입니다. 꺼져 있는 동안 이 모드는 레벨업 보상에 대해 아무것도 주장하지 않습니다.",
    "默认关闭。关闭时本模组对升级奖励不做任何干预。",
    "По умолчанию выключено. Пока выключено, мод ничего не утверждает о наградах за уровень.",
    "Standardmäßig aus. Solange es aus ist, behauptet dieser Mod nichts über Stufenbelohnungen.",
    "Désactivé par défaut. Tant que c'est désactivé, ce mod n'impose rien sur les récompenses de niveau.",
    "Desactivado por defecto. Mientras lo esté, este mod no impone nada sobre las recompensas de nivel.",
    "Disattivato per impostazione predefinita. Finché è disattivato, questa mod non impone nulla sulle ricompense di livello.",
    "Domyślnie wyłączone. Gdy jest wyłączone, mod niczego nie narzuca nagrodom za awans.",
    "Ve výchozím stavu vypnuto. Dokud je vypnuto, mod o odměnách za postup nic netvrdí.")
TRANSLATIONS["CPC_LvlUp_SecPerks"] = T(
    "レベルごとのパークポイント", "레벨당 특전 점수", "每级技能点", "Очки способностей за уровень",
    "Vorteilspunkte pro Stufe", "Points d'aptitude par niveau", "Puntos de habilidad especial por nivel",
    "Punti privilegio per livello", "Punkty atutów na poziom", "Body vlastností za úroveň")
TRANSLATIONS["CPC_LvlUp_PerkIntro"] = T(
    "整数のパークポイントを、レベルごとの表として指定します: 記載したレベル以降、レベルアップごとにその数を与えます。バニラは 1 行 - レベル 1 から 1 パークです。",
    "정수 특전 점수를 레벨별 표로 지정합니다: 표에 적힌 레벨부터 레벨업마다 그 수만큼 줍니다. 바닐라는 한 줄 - 레벨 1부터 1 특전입니다.",
    "以整数技能点按等级列表设置：从列出的每个等级起，每次升级给予该数量。原版只有一行 - 从 1 级起，每级 1 点。",
    "Целые очки способностей, таблицей по уровням: начиная с каждого указанного уровня столько за повышение. В оригинале одна строка - с уровня 1, 1 очко.",
    "Ganze Vorteilspunkte als Tabelle nach Stufe: ab jeder aufgeführten Stufe so viele je Aufstieg. Original ist eine Zeile - ab Stufe 1 ein Punkt.",
    "Des points d'aptitude entiers, sous forme de table par niveau : à partir de chaque niveau listé, ce nombre par passage de niveau. Le jeu de base tient en une ligne - à partir du niveau 1, 1 point.",
    "Puntos de habilidad especial enteros, como una tabla por nivel: desde cada nivel listado, esa cantidad por subida. El juego base es una sola fila: desde el nivel 1, 1 punto.",
    "Punti privilegio interi, come tabella per livello: da ogni livello elencato in poi, quel numero per passaggio di livello. Il gioco base è una riga sola - dal livello 1, 1 punto.",
    "Całkowite punkty atutów jako tabela według poziomów: od każdego wypisanego poziomu tyle na awans. Podstawowa gra to jeden wiersz - od poziomu 1, 1 punkt.",
    "Celé body vlastností jako tabulka podle úrovně: od každé uvedené úrovně tolik za postup. Základní hra má jeden řádek - od úrovně 1 jeden bod.")
TRANSLATIONS["CPC_LvlUp_FromLevel"] = T(
    "このレベルから", "이 레벨부터", "起始等级", "С уровня", "Ab Stufe",
    "À partir du niveau", "Desde el nivel", "Dal livello", "Od poziomu", "Od úrovně")
TRANSLATIONS["CPC_LvlUp_Perks"] = T(
    "パーク", "특전", "技能点", "Очки", "Vorteile", "Aptitudes", "Puntos", "Privilegi", "Atuty", "Vlastnosti")
TRANSLATIONS["CPC_LvlUp_Remove"] = T(
    "削除", "삭제", "删除", "Удалить", "Entfernen", "Supprimer", "Eliminar", "Rimuovi", "Usuń", "Odebrat")
TRANSLATIONS["CPC_LvlUp_AddRow"] = T(
    "行を追加", "행 추가", "添加一行", "Добавить строку", "Zeile hinzufügen",
    "Ajouter une ligne", "Añadir una fila", "Aggiungi una riga", "Dodaj wiersz", "Přidat řádek")
TRANSLATIONS["CPC_LvlUp_HelpRows"] = T(
    "到達したレベル以下で最後の行が適用されます。各行は整数のパーク数です。",
    "도달한 레벨 이하의 마지막 행이 적용됩니다. 각 행은 정수 특전 수입니다.",
    "适用达到等级处或之下的最后一行。每行都是整数个技能点。",
    "Применяется последняя строка на достигнутом уровне или ниже. В каждой строке целое число очков.",
    "Es gilt die letzte Zeile auf oder unter der erreichten Stufe. Jede Zeile ist eine ganze Zahl an Vorteilen.",
    "C'est la dernière ligne au niveau atteint ou en dessous qui s'applique. Chaque ligne est un nombre entier d'aptitudes.",
    "Se aplica la última fila igual o inferior al nivel alcanzado. Cada fila es un número entero de puntos.",
    "Si applica l'ultima riga pari o inferiore al livello raggiunto. Ogni riga è un numero intero di privilegi.",
    "Stosuje się ostatni wiersz na osiągniętym poziomie lub niżej. Każdy wiersz to całkowita liczba atutów.",
    "Použije se poslední řádek na dosažené úrovni nebo pod ní. Každý řádek je celé číslo vlastností.")
TRANSLATIONS["CPC_LvlUp_SeeOtherTabs"] = T(
    "レベルアップで得られる体力・マジカ・スタミナは Attributes タブに、選択ごとの所持重量は Carry Weight タブにあります。どちらもこれがオンの間に適用されます。",
    "레벨업으로 얻는 체력·매지카·스태미나는 Attributes 탭에, 선택별 소지 중량은 Carry Weight 탭에 있습니다. 둘 다 이 옵션이 켜져 있을 때 적용됩니다.",
    "升级给予的生命、法力和体力在 Attributes 标签页，按选择给予的负重在 Carry Weight 标签页；两者都在此项开启时生效。",
    "Здоровье, магия и запас сил за уровень настраиваются на вкладке Attributes, а переносимый вес за выбор - на вкладке Carry Weight; оба применяются, пока это включено.",
    "Die Gesundheit, Magicka und Ausdauer eines Stufenaufstiegs stehen auf der Registerkarte Attributes, die Tragkraft je Wahl auf Carry Weight; beide gelten, solange dies eingeschaltet ist.",
    "La santé, la magie et la vigueur accordées par un niveau se trouvent dans l'onglet Attributes, et la charge par choix dans l'onglet Carry Weight ; les deux s'appliquent tant que ceci est activé.",
    "La salud, magia y aguante que da un nivel están en la pestaña Attributes, y la carga por elección en Carry Weight; ambas se aplican mientras esto esté activado.",
    "La salute, magicka e vigore dati da un livello sono nella scheda Attributes, e il carico per scelta nella scheda Carry Weight; entrambi si applicano finché questo è attivo.",
    "Zdrowie, magia i kondycja przyznawane za poziom są na karcie Attributes, a udźwig za wybór na karcie Carry Weight; oba działają, gdy ta opcja jest włączona.",
    "Zdraví, magicka a výdrž dané postupem jsou na kartě Attributes a nosnost za volbu na kartě Carry Weight; obojí platí, dokud je toto zapnuté.")

# --- Attributes ----------------------------------------------------------------------------------
TRANSLATIONS["CPC_Attr_Intro"] = T(
    "初期の体力・マジカ・スタミナと、それを選んだレベルアップごとの上昇量です。ゲームはその選択の回数を記録しないため、この MOD はキャラクターに導入された時点からすべて自分で数えます。",
    "시작 체력·매지카·스태미나와, 그것을 선택한 레벨업마다의 상승량입니다. 게임은 그 선택 횟수를 기록하지 않으므로, 이 모드는 캐릭터에 설치된 순간부터 모두 직접 셉니다.",
    "初始生命、法力和体力，以及在升级中选择它们时各自的增长量。游戏并不记录这些选择，因此本模组从被安装到该角色的那一刻起自行统计每一次选择。",
    "Начальные здоровье, магия и запас сил, и сколько каждый получает при повышении уровня, где вы его выбираете. Игра не считает эти выборы, поэтому мод считает каждый сам с момента установки на персонажа.",
    "Anfangswerte für Gesundheit, Magicka und Ausdauer und was jeder bei einem Stufenaufstieg gewinnt, bei dem du ihn wählst. Das Spiel zählt diese Entscheidungen nicht, deshalb zählt dieser Mod jede selbst, ab dem Moment, in dem er auf einem Charakter installiert ist.",
    "Santé, magie et vigueur de départ, et ce que chacune gagne lors d'un passage de niveau où vous la choisissez. Le jeu ne compte pas ces choix, alors ce mod les compte lui-même dès son installation sur un personnage.",
    "Salud, magia y aguante iniciales, y lo que gana cada uno en una subida de nivel en la que lo eliges. El juego no lleva la cuenta de esas elecciones, así que este mod cuenta cada una por su cuenta desde el momento en que se instala en un personaje.",
    "Salute, magicka e vigore iniziali e quanto guadagna ciascuno a un passaggio di livello in cui lo scegli. Il gioco non tiene conto di quelle scelte, quindi questa mod le conta da sé dal momento in cui è installata su un personaggio.",
    "Początkowe zdrowie, magia i kondycja oraz to, ile każde z nich zyskuje przy awansie, w którym je wybierzesz. Gra nie zlicza tych wyborów, więc ten mod liczy każdy sam od chwili instalacji na postaci.",
    "Počáteční zdraví, magicka a výdrž a kolik každé získá při postupu, kde ho zvolíš. Hra tyto volby nepočítá, takže je tento mod počítá sám od chvíle, kdy je na postavě nainstalován.")
TRANSLATIONS["CPC_Attr_Inert"] = T(
    "能力値の選択は数えられておらず、レベルアップはバニラどおりの量を与えます",
    "능력치 선택이 집계되지 않으며, 레벨업은 바닐라와 같은 양을 줍니다",
    "属性选择未被统计，升级给予的量与原版相同",
    "выборы атрибутов не считаются, и повышение уровня даёт то же, что в оригинале",
    "Attributentscheidungen werden nicht gezählt und ein Stufenaufstieg gibt, was das Original gibt",
    "les choix d'attributs ne sont pas comptés et un niveau accorde ce que le jeu de base accorde",
    "las elecciones de atributo no se cuentan y subir de nivel da lo que da el juego base",
    "le scelte degli attributi non vengono contate e un livello concede quanto concede il gioco base",
    "wybory atrybutów nie są liczone, a awans daje to, co podstawowa gra",
    "volby atributů se nepočítají a postup dává to, co základní hra")
TRANSLATIONS["CPC_Attr_Control"] = T(
    "初期能力値を制御する", "시작 능력치 제어", "控制初始属性", "Управлять начальными атрибутами",
    "Startattribute steuern", "Contrôler les attributs de départ", "Controlar los atributos iniciales",
    "Controlla gli attributi iniziali", "Kontroluj początkowe atrybuty", "Řídit počáteční atributy")
TRANSLATIONS["CPC_Attr_HelpControl"] = T(
    "既定はオフです。オンにすると、下の各初期値が種族本来の初期値の上にこの MOD の恒久修正として適用され、オフに戻すと取り除かれます。オフの間は何も主張しません。",
    "기본값은 꺼짐입니다. 켜면 아래 각 시작값이 종족 고유의 시작값 위에 이 모드의 영구 보정으로 적용되고, 다시 끄면 제거됩니다. 꺼져 있는 동안에는 아무것도 주장하지 않습니다.",
    "默认关闭。开启后，下面的每个初始值都会作为本模组的永久修正叠加在你种族自身的初始值之上；再关闭时会被撤销。关闭期间不做任何干预。",
    "По умолчанию выключено. Включено: каждое начальное значение ниже накладывается поверх собственного старта вашей расы как постоянный модификатор мода и снимается, если вы выключите это. Пока выключено, ничего не утверждается.",
    "Standardmäßig aus. An: Jeder Startwert unten wird als dauerhafter Modifikator dieses Mods auf den eigenen Start deiner Rasse gelegt und wieder entfernt, wenn du es ausschaltest. Solange es aus ist, wird nichts behauptet.",
    "Désactivé par défaut. Activé : chaque valeur de départ ci-dessous s'ajoute au départ propre à votre race comme modificateur permanent de ce mod, et est retirée si vous désactivez. Tant que c'est désactivé, rien n'est imposé.",
    "Desactivado por defecto. Activado: cada valor inicial de abajo se aplica sobre el inicio propio de tu raza como modificador permanente de este mod, y se retira si lo desactivas. Mientras esté apagado no se impone nada.",
    "Disattivato per impostazione predefinita. Attivo: ogni valore iniziale sotto viene applicato sopra l'inizio proprio della tua razza come modificatore permanente di questa mod e viene tolto se lo disattivi. Finché è spento non viene imposto nulla.",
    "Domyślnie wyłączone. Włączone: każda wartość początkowa poniżej jest nakładana na własny start twojej rasy jako trwały modyfikator tego moda i zostaje zdjęta, gdy to wyłączysz. Gdy wyłączone, nic nie jest narzucane.",
    "Ve výchozím stavu vypnuto. Zapnuto: každá počáteční hodnota níže se přidá na vlastní start tvé rasy jako trvalý modifikátor tohoto modu a při vypnutí se zase odebere. Dokud je vypnuto, nic se netvrdí.")
TRANSLATIONS["CPC_Attr_SecStarting"] = T(
    "初期値", "시작 값", "初始数值", "Начальные значения", "Startwerte",
    "Valeurs de départ", "Valores iniciales", "Valori iniziali", "Wartości początkowe", "Počáteční hodnoty")
TRANSLATIONS["CPC_Attr_StartHealth"] = T(
    "初期体力", "시작 체력", "初始生命", "Начальное здоровье", "Anfangsgesundheit",
    "Santé de départ", "Salud inicial", "Salute iniziale", "Początkowe zdrowie", "Počáteční zdraví")
TRANSLATIONS["CPC_Attr_StartMagicka"] = T(
    "初期マジカ", "시작 매지카", "初始法力", "Начальная магия", "Anfangsmagicka",
    "Magie de départ", "Magia inicial", "Magicka iniziale", "Początkowa magia", "Počáteční magicka")
TRANSLATIONS["CPC_Attr_StartStamina"] = T(
    "初期スタミナ", "시작 스태미나", "初始体力", "Начальный запас сил", "Anfangsausdauer",
    "Vigueur de départ", "Aguante inicial", "Vigore iniziale", "Początkowa kondycja", "Počáteční výdrž")
TRANSLATIONS["CPC_Attr_HelpStarting"] = T(
    "バニラの初期値が 100 のキャラクターにとっての、その能力値の開始値です。初期値がこれより高い/低い種族はその差を保ちます: 値は (これ - 100) としてキャラクターの初期値の上に適用されます。",
    "바닐라 시작값이 100인 캐릭터 기준으로 그 능력치가 시작하는 값입니다. 시작값이 더 높거나 낮은 종족은 그 차이를 유지합니다: 값은 (이 값 - 100)으로 캐릭터의 시작값 위에 적용됩니다.",
    "对于原版初始值为 100 的角色而言，该属性的起始数值。起始更高或更低的种族会保留其差值：数值以 (此值 - 100) 的形式叠加在角色的初始值之上。",
    "С чего начинается атрибут для персонажа, чей оригинальный старт равен 100. Раса с более высоким или низким стартом сохраняет свою разницу: значение накладывается как (это - 100) поверх того, с чем персонаж начал.",
    "Womit das Attribut beginnt, für einen Charakter, dessen Original-Start 100 beträgt. Eine Rasse mit höherem oder niedrigerem Start behält ihren Unterschied: der Wert wird als (dies - 100) auf den Start des Charakters gelegt.",
    "La valeur de départ de l'attribut, pour un personnage dont le départ de base est 100. Une race qui commence plus haut ou plus bas conserve sa différence : la valeur est appliquée comme (ceci - 100) par-dessus le départ du personnage.",
    "Con cuánto empieza el atributo, para un personaje cuyo inicio original es 100. Una raza que empieza más alto o más bajo mantiene su diferencia: el valor se aplica como (esto - 100) sobre lo que el personaje tenía al empezar.",
    "Da quanto parte l'attributo, per un personaggio il cui inizio base è 100. Una razza che parte più in alto o più in basso mantiene la sua differenza: il valore è applicato come (questo - 100) sopra ciò con cui il personaggio è partito.",
    "Od jakiej wartości zaczyna się atrybut dla postaci, której podstawowy start wynosi 100. Rasa startująca wyżej lub niżej zachowuje swoją różnicę: wartość jest nakładana jako (to - 100) na to, z czym postać zaczęła.",
    "Od kolika atribut začíná, pro postavu, jejíž základní start je 100. Rasa, která začíná výš nebo níž, si svůj rozdíl ponechá: hodnota se použije jako (toto - 100) na to, s čím postava začala.")
TRANSLATIONS["CPC_Attr_SecGain"] = T(
    "レベルアップごとの上昇量", "레벨업당 상승량", "每次升级的增长", "Прирост за уровень",
    "Zuwachs pro Stufenaufstieg", "Gain par passage de niveau", "Ganancia por subida de nivel",
    "Guadagno per passaggio di livello", "Przyrost na awans", "Přírůstek za postup")
TRANSLATIONS["CPC_Attr_HealthPerLevel"] = T(
    "レベルごとの体力", "레벨당 체력", "每级生命", "Здоровья за уровень", "Gesundheit pro Stufe",
    "Santé par niveau", "Salud por nivel", "Salute per livello", "Zdrowie na poziom", "Zdraví za úroveň")
TRANSLATIONS["CPC_Attr_MagickaPerLevel"] = T(
    "レベルごとのマジカ", "레벨당 매지카", "每级法力", "Магии за уровень", "Magicka pro Stufe",
    "Magie par niveau", "Magia por nivel", "Magicka per livello", "Magia na poziom", "Magicka za úroveň")
TRANSLATIONS["CPC_Attr_StaminaPerLevel"] = T(
    "レベルごとのスタミナ", "레벨당 스태미나", "每级体力", "Запаса сил за уровень", "Ausdauer pro Stufe",
    "Vigueur par niveau", "Aguante por nivel", "Vigore per livello", "Kondycja na poziom", "Výdrž za úroveň")
TRANSLATIONS["CPC_Attr_HelpGain"] = T(
    "選んだ能力値が得る量です。バニラはいずれも 10 です。",
    "선택한 능력치가 얻는 양입니다. 바닐라는 각각 10입니다.",
    "所选属性获得的数值。原版各为 10。",
    "Сколько получает выбранный атрибут. В оригинале по 10.",
    "Was das gewählte Attribut gewinnt. Original sind es jeweils 10.",
    "Ce que gagne l'attribut choisi. Le jeu de base donne 10 pour chacun.",
    "Lo que gana el atributo elegido. El juego base da 10 para cada uno.",
    "Quanto guadagna l'attributo scelto. Il gioco base dà 10 per ciascuno.",
    "Ile zyskuje wybrany atrybut. Podstawowa gra daje po 10.",
    "Kolik získá zvolený atribut. Základní hra dává po 10.")
TRANSLATIONS["CPC_Attr_NeedsLevelUpControl"] = T(
    "Level Up タブの「レベルアップの報酬を制御する」がオンのときに適用されます。今はオフなので、レベルアップはバニラの 10 を与えます。",
    "Level Up 탭의 \"레벨업 보상 제어\"가 켜져 있을 때 적용됩니다. 지금은 꺼져 있으므로 레벨업은 바닐라의 10을 줍니다.",
    "仅在 Level Up 标签页的“控制升级奖励”开启时生效。目前它是关闭的，因此升级给予原版的 10。",
    "Применяется, пока на вкладке Level Up включено \"Управлять наградой за уровень\". Сейчас выключено, поэтому уровень даёт оригинальные 10.",
    "Gilt, solange auf der Registerkarte Level Up \"Belohnung eines Stufenaufstiegs steuern\" an ist. Es ist aus, also gibt ein Aufstieg die originalen 10.",
    "S'applique tant que « Contrôler ce qu'accorde un niveau » est activé dans l'onglet Level Up. C'est désactivé, donc un niveau donne les 10 du jeu de base.",
    "Se aplica mientras \"Controlar lo que da subir de nivel\" esté activado en la pestaña Level Up. Está apagado, así que un nivel da los 10 originales.",
    "Si applica finché \"Controlla cosa concede un passaggio di livello\" è attivo nella scheda Level Up. È spento, quindi un livello dà i 10 del gioco base.",
    "Działa, gdy na karcie Level Up włączone jest \"Kontroluj nagrody za awans\". Jest wyłączone, więc awans daje podstawowe 10.",
    "Platí, dokud je na kartě Level Up zapnuto \"Řídit odměny za postup na úroveň\". Je vypnuto, takže postup dává základních 10.")
TRANSLATIONS["CPC_Attr_Row"] = T(
    "%s: 基礎 %.0f、本MOD %+.0f、選択による %+.0f (%u %s) = 恒久 %.0f   (現在 %.0f)",
    "%s: 기본 %.0f, 이 모드 %+.0f, 선택으로 %+.0f (%u %s) = 영구 %.0f   (현재 %.0f)",
    "%s：基础 %.0f，本模组 %+.0f，选择所得 %+.0f (%u %s) = 永久 %.0f   (当前 %.0f)",
    "%s: база %.0f, от мода %+.0f, %+.0f за %u %s = постоянно %.0f   (сейчас %.0f)",
    "%s: %.0f Basis, %+.0f von diesem Mod, %+.0f aus %u %s = %.0f dauerhaft   (jetzt %.0f)",
    "%s : %.0f de base, %+.0f de ce mod, %+.0f sur %u %s = %.0f permanent   (actuellement %.0f)",
    "%s: %.0f de base, %+.0f de este mod, %+.0f en %u %s = %.0f permanente   (ahora %.0f)",
    "%s: %.0f base, %+.0f da questa mod, %+.0f su %u %s = %.0f permanente   (ora %.0f)",
    "%s: baza %.0f, od moda %+.0f, %+.0f z %u %s = trwale %.0f   (teraz %.0f)",
    "%s: základ %.0f, od modu %+.0f, %+.0f z %u %s = trvale %.0f   (nyní %.0f)")
TRANSLATIONS["CPC_Attr_Investment"] = T(
    "回の選択", "회 선택", "次投入", "вложение", "Entscheidung", "choix", "elección", "scelta", "wybór", "volbu")
TRANSLATIONS["CPC_Attr_Investments"] = T(
    "回の選択", "회 선택", "次投入", "вложений", "Entscheidungen", "choix", "elecciones", "scelte", "wyborów", "voleb")
TRANSLATIONS["CPC_Attr_SinceLevel"] = T(
    "レベル %u 以降を計上しています。この MOD がこのキャラクターを最初に見た時点です。それ以前の %u 回のレベルアップは不明であり、推測もしません: キャラクターが持っていた他のものと同じく基礎値の一部として扱われます。",
    "레벨 %u부터 집계했습니다. 이 모드가 이 캐릭터를 처음 본 시점입니다. 그 이전의 %u 회 레벨업은 알 수 없으며 추측하지도 않습니다: 캐릭터가 가지고 있던 다른 것들과 함께 기본값의 일부로 취급됩니다.",
    "自等级 %u 起开始统计，那是本模组第一次见到这个角色的时刻。此前的 %u 次升级无从得知，也不会去猜：它们与角色原有的一切一同算作基础值的一部分。",
    "Счёт ведётся с уровня %u, когда мод впервые увидел этого персонажа. Предыдущие %u повышений неизвестны, и их не угадывают: они входят в базу вместе со всем прочим, с чем персонаж начал.",
    "Gezählt ab Stufe %u, als dieser Mod diesen Charakter zum ersten Mal gesehen hat. Die %u früheren Aufstiege sind unbekannt und werden nicht geraten: sie gehören zur Basis, zusammen mit allem anderen, was der Charakter mitbrachte.",
    "Compté depuis le niveau %u, quand ce mod a vu ce personnage pour la première fois. Les %u passages de niveau antérieurs sont inconnus et ne sont pas devinés : ils font partie de la base, avec tout ce que le personnage avait déjà.",
    "Contado desde el nivel %u, cuando este mod vio a este personaje por primera vez. Las %u subidas anteriores son desconocidas y no se adivinan: forman parte de la base, junto con todo lo demás con lo que empezó el personaje.",
    "Contato dal livello %u, quando questa mod ha visto per la prima volta questo personaggio. I %u passaggi di livello precedenti sono ignoti e non vengono indovinati: fanno parte della base, insieme a tutto ciò con cui il personaggio è partito.",
    "Liczone od poziomu %u, gdy ten mod po raz pierwszy zobaczył tę postać. Wcześniejsze %u awansów jest nieznanych i nie są zgadywane: wchodzą do bazy razem ze wszystkim innym, z czym postać zaczęła.",
    "Počítáno od úrovně %u, kdy tento mod tuto postavu poprvé viděl. Dřívějších %u postupů není známo a neodhadují se: patří do základu spolu se vším ostatním, s čím postava začala.")
TRANSLATIONS["CPC_Attr_SinceLevelOne"] = T(
    "レベル 1 から計上しています - 履歴はすべて把握しています。",
    "레벨 1부터 집계했습니다 - 전체 이력을 알고 있습니다.",
    "自等级 1 起统计 - 全部历史都已知。",
    "Счёт с уровня 1 - вся история известна.",
    "Gezählt ab Stufe 1 - die ganze Geschichte ist bekannt.",
    "Compté depuis le niveau 1 - tout l'historique est connu.",
    "Contado desde el nivel 1: se conoce todo el historial.",
    "Contato dal livello 1 - l'intera storia è nota.",
    "Liczone od poziomu 1 - cała historia jest znana.",
    "Počítáno od úrovně 1 - celá historie je známa.")
TRANSLATIONS["CPC_Attr_NotCounting"] = T(
    "計上していません: レベルアップパッチが適用されていません (パッチタブを参照)。",
    "집계하지 않습니다: 레벨업 패치가 붙어 있지 않습니다 (패치 탭 참조).",
    "未在统计：升级补丁未挂接（见补丁标签页）。",
    "Не считается: патч повышения уровня не подключён (см. вкладку патчей).",
    "Wird nicht gezählt: der Stufenaufstiegs-Patch ist nicht eingehängt (siehe Registerkarte Patches).",
    "Pas de comptage : le correctif de passage de niveau n'est pas attaché (voir l'onglet Patches).",
    "No se está contando: el parche de subida de nivel no está enganchado (mira la pestaña Patches).",
    "Non si sta contando: la patch del passaggio di livello non è agganciata (vedi la scheda Patches).",
    "Nie liczone: łatka awansu nie jest podpięta (zobacz kartę Patches).",
    "Nepočítá se: záplata postupu na úroveň není připojena (viz karta Patches).")
TRANSLATIONS["CPC_Attr_HelpApply"] = T(
    "初期値を再適用し、表示を更新します。セーブ読み込み時、レベルアップごと、上の項目を変更したときにも自動で実行されます - 常駐処理はありません。",
    "시작값을 다시 적용하고 표시를 새로 고칩니다. 세이브를 불러올 때, 레벨업마다, 위 항목을 바꿀 때도 자동으로 실행됩니다 - 백그라운드 처리는 없습니다.",
    "重新应用初始值并刷新显示。读取存档时、每次升级时以及你更改上面任何内容时也会自动执行 - 没有后台轮询。",
    "Заново применяет начальные значения и обновляет сводку. Также срабатывает само при загрузке сохранения, при каждом повышении уровня и при изменении чего-либо выше - ничего не работает в фоне.",
    "Wendet die Startwerte erneut an und aktualisiert die Anzeige. Läuft außerdem von selbst beim Laden eines Spielstands, bei jedem Stufenaufstieg und wenn du oben etwas änderst - nichts läuft im Hintergrund.",
    "Réapplique les valeurs de départ et rafraîchit l'affichage. S'exécute aussi tout seul au chargement d'une sauvegarde, à chaque passage de niveau et quand vous modifiez quelque chose ci-dessus - rien ne tourne en arrière-plan.",
    "Vuelve a aplicar los valores iniciales y actualiza la lectura. También se ejecuta solo al cargar una partida, en cada subida de nivel y cuando cambias algo arriba: nada corre en segundo plano.",
    "Riapplica i valori iniziali e aggiorna il riepilogo. Viene eseguito anche da solo al caricamento di un salvataggio, a ogni passaggio di livello e quando modifichi qualcosa sopra - nulla gira in background.",
    "Ponownie stosuje wartości początkowe i odświeża odczyt. Uruchamia się też samo przy wczytaniu zapisu, przy każdym awansie i gdy zmienisz cokolwiek powyżej - nic nie działa w tle.",
    "Znovu použije počáteční hodnoty a obnoví výpis. Spustí se také samo při načtení pozice, při každém postupu a když nahoře něco změníš - nic neběží na pozadí.")

# --- Carry Weight --------------------------------------------------------------------------------
TRANSLATIONS["CPC_CW_Intro"] = T(
    "所持重量をレベルの式として扱います: 初期値 + レベルごと x (レベル - 1) を、恒久的な所持重量に適用します (付呪や魔法の効果には触れません)。セーブ読み込み時、レベルアップ時、ここで値を変えたとき、そして「今すぐ適用」を押したときに再計算されます - 常駐処理はありません。",
    "소지 중량을 레벨의 식으로 다룹니다: 시작값 + 레벨당 x (레벨 - 1)을 영구 소지 중량에 적용합니다(마법부여와 주문 효과는 건드리지 않습니다). 세이브를 불러올 때, 레벨업할 때, 여기서 값을 바꿀 때, 그리고 \"지금 적용\"을 누를 때 다시 계산됩니다 - 백그라운드 처리는 없습니다.",
    "把负重当作等级的公式：初始值 + 每级值 x (等级 - 1)，应用到你的永久负重上（附魔与法术效果不受影响）。在读取存档、升级、你在此更改数值以及按下“立即应用”时重新计算 - 没有后台轮询。",
    "Переносимый вес как формула от уровня: начальное + за уровень x (уровень - 1), применяется к постоянному переносимому весу (зачарования и заклинания не трогаются). Пересчитывается при загрузке сохранения, при повышении уровня, при изменении значения здесь и при нажатии \"Применить сейчас\" - ничего не работает в фоне.",
    "Tragkraft als Formel deiner Stufe: Start + pro Stufe x (Stufe - 1), angewendet auf deine dauerhafte Tragkraft (Verzauberungen und Zauber bleiben unberührt). Neu berechnet beim Laden eines Spielstands, bei einem Stufenaufstieg, wenn du hier einen Wert änderst und wenn du \"Jetzt anwenden\" drückst - nichts läuft im Hintergrund.",
    "La charge comme une formule de votre niveau : départ + par niveau x (niveau - 1), appliquée à votre charge permanente (les enchantements et les sorts ne sont pas touchés). Recalculée au chargement d'une sauvegarde, à un passage de niveau, quand vous changez une valeur ici et quand vous appuyez sur « Appliquer maintenant » - rien ne tourne en arrière-plan.",
    "La carga como una fórmula de tu nivel: inicial + por nivel x (nivel - 1), aplicada a tu carga permanente (los encantamientos y los hechizos no se tocan). Se recalcula al cargar una partida, al subir de nivel, al cambiar un valor aquí y al pulsar \"Aplicar ahora\": nada corre en segundo plano.",
    "Il carico come formula del tuo livello: iniziale + per livello x (livello - 1), applicato al tuo carico permanente (incantesimi e magie non vengono toccati). Ricalcolato al caricamento di un salvataggio, a un passaggio di livello, quando cambi un valore qui e quando premi \"Applica ora\" - nulla gira in background.",
    "Udźwig jako wzór od twojego poziomu: początkowy + na poziom x (poziom - 1), stosowany do twojego trwałego udźwigu (zaklęcia i czary pozostają nietknięte). Przeliczany przy wczytaniu zapisu, przy awansie, przy zmianie wartości tutaj i po naciśnięciu \"Zastosuj teraz\" - nic nie działa w tle.",
    "Nosnost jako vzorec podle úrovně: počáteční + za úroveň x (úroveň - 1), použito na tvou trvalou nosnost (očarování a kouzla zůstávají nedotčena). Přepočítá se při načtení pozice, při postupu, při změně hodnoty zde a při stisku \"Použít nyní\" - nic neběží na pozadí.")
TRANSLATIONS["CPC_CW_Control"] = T(
    "所持重量を制御する", "소지 중량 제어", "控制负重", "Управлять переносимым весом",
    "Tragkraft steuern", "Contrôler la charge", "Controlar la carga", "Controlla il carico",
    "Kontroluj udźwig", "Řídit nosnost")
TRANSLATIONS["CPC_CW_HelpControl"] = T(
    "既定はオフです。オフの間は何も主張せず、この MOD が加えていた分も取り除かれます。",
    "기본값은 꺼짐입니다. 꺼져 있는 동안에는 아무것도 주장하지 않으며, 이 모드가 더했던 몫도 제거됩니다.",
    "默认关闭。关闭时不做任何干预，本模组此前添加的部分也会被撤销。",
    "По умолчанию выключено. Пока выключено, ничего не утверждается, а всё добавленное модом снимается.",
    "Standardmäßig aus. Solange es aus ist, wird nichts behauptet, und was dieser Mod hinzugefügt hatte, wird wieder entfernt.",
    "Désactivé par défaut. Tant que c'est désactivé, rien n'est imposé et ce que ce mod avait ajouté est retiré.",
    "Desactivado por defecto. Mientras lo esté, no se impone nada y lo que este mod hubiera añadido se retira.",
    "Disattivato per impostazione predefinita. Finché è spento non viene imposto nulla e quanto questa mod aveva aggiunto viene tolto.",
    "Domyślnie wyłączone. Gdy jest wyłączone, nic nie jest narzucane, a to, co mod dodał, zostaje zdjęte.",
    "Ve výchozím stavu vypnuto. Dokud je vypnuto, nic se netvrdí a to, co mod přidal, se zase odebere.")
TRANSLATIONS["CPC_CW_StandingDown"] = T(
    "待機中: %s。その間、ここの設定は適用されません。",
    "대기 중: %s. 그동안 여기의 설정은 적용되지 않습니다.",
    "已让位：%s。在此期间此处的设置不会生效。",
    "Уступаем: %s. Пока это так, здесь ничего не применяется.",
    "Zurückgestellt: %s. Solange das gilt, wird hier nichts angewendet.",
    "En retrait : %s. Tant que c'est le cas, rien n'est appliqué ici.",
    "Cediendo el paso: %s. Mientras sea así, aquí no se aplica nada.",
    "In disparte: %s. Finché è così, qui non viene applicato nulla.",
    "Ustępujemy: %s. Dopóki tak jest, nic tutaj nie jest stosowane.",
    "Ustupujeme: %s. Dokud to platí, zde se nic nepoužije.")
TRANSLATIONS["CPC_CW_SecFormula"] = T(
    "計算式", "계산식", "公式", "Формула", "Die Formel", "La formule", "La fórmula", "La formula", "Wzór", "Vzorec")
TRANSLATIONS["CPC_CW_Starting"] = T(
    "初期所持重量", "시작 소지 중량", "初始负重", "Начальный переносимый вес", "Anfangstragkraft",
    "Charge de départ", "Carga inicial", "Carico iniziale", "Początkowy udźwig", "Počáteční nosnost")
TRANSLATIONS["CPC_CW_HelpStarting"] = T(
    "レベル 1 での所持重量です。バニラは 300 です。",
    "레벨 1에서의 소지 중량입니다. 바닐라는 300입니다.",
    "等级 1 时的负重。原版为 300。",
    "Переносимый вес на уровне 1. В оригинале 300.",
    "Tragkraft auf Stufe 1. Original sind 300.",
    "La charge au niveau 1. Le jeu de base donne 300.",
    "La carga en el nivel 1. El juego base son 300.",
    "Il carico al livello 1. Il gioco base è 300.",
    "Udźwig na 1. poziomie. Podstawowa gra to 300.",
    "Nosnost na úrovni 1. V základní hře 300.")
TRANSLATIONS["CPC_CW_PerLevel"] = T(
    "レベルごと", "레벨당", "每级", "За уровень", "Pro Stufe",
    "Par niveau", "Por nivel", "Per livello", "Na poziom", "Za úroveň")
TRANSLATIONS["CPC_CW_HelpPerLevel"] = T(
    "最初のレベル以降、どの能力値を選んでもレベルごとに加算されます。",
    "첫 레벨 이후 어떤 능력치를 선택하든 레벨마다 더해집니다.",
    "第一级之后每一级都会追加，无论你选择哪个属性。",
    "Добавляется за каждый уровень после первого, какой бы атрибут вы ни выбрали.",
    "Wird für jede Stufe nach der ersten addiert, egal welches Attribut du wählst.",
    "S'ajoute pour chaque niveau après le premier, quel que soit l'attribut choisi.",
    "Se suma por cada nivel después del primero, elijas el atributo que elijas.",
    "Si aggiunge per ogni livello dopo il primo, qualunque attributo tu scelga.",
    "Doliczane za każdy poziom po pierwszym, niezależnie od wybranego atrybutu.",
    "Přičítá se za každou úroveň po první, ať zvolíš kterýkoli atribut.")
TRANSLATIONS["CPC_CW_SecPerChoice"] = T(
    "レベルアップの選択ごと", "레벨업 선택별", "按升级时的选择", "За выбор при повышении уровня",
    "Je Wahl beim Stufenaufstieg", "Par choix au passage de niveau", "Por elección al subir de nivel",
    "Per scelta al passaggio di livello", "Za wybór przy awansie", "Za volbu při postupu")
TRANSLATIONS["CPC_CW_OffWhileFormula"] = T(
    "計算式が所持重量を制御している間は無効です: 計算式が合計を決めるため、選択ごとの加算は次の再計算で打ち消されます。",
    "계산식이 소지 중량을 제어하는 동안에는 꺼집니다: 계산식이 총합을 정하므로, 선택별 증가분은 다음 재계산에서 사라집니다.",
    "当公式控制负重时无效：公式会设定总量，因此按选择的增量会在下一次重算时被抹去。",
    "Отключено, пока формула управляет переносимым весом: она задаёт итог, поэтому прибавка за выбор была бы стёрта при следующем пересчёте.",
    "Aus, solange die Formel die Tragkraft steuert: sie setzt die Gesamtsumme, ein Zuwachs je Wahl würde beim nächsten Neuberechnen wieder verschwinden.",
    "Inactif tant que la formule contrôle la charge : elle fixe le total, donc un gain par choix serait annulé au prochain recalcul.",
    "Apagado mientras la fórmula controla la carga: fija el total, así que una ganancia por elección se deshaaría en el siguiente recálculo.",
    "Disattivo finché la formula controlla il carico: imposta il totale, quindi un guadagno per scelta verrebbe annullato al ricalcolo successivo.",
    "Wyłączone, gdy udźwigiem steruje wzór: ustala on sumę, więc przyrost za wybór zostałby cofnięty przy następnym przeliczeniu.",
    "Vypnuto, dokud nosnost řídí vzorec: ten určuje celek, takže přírůstek za volbu by se při dalším přepočtu ztratil.")
TRANSLATIONS["CPC_CW_PerHealth"] = T(
    "...体力を選んだとき", "...체력을 선택했을 때", "...选择生命时", "...когда выбрано здоровье",
    "... wenn Gesundheit gewählt wird", "... quand la santé est choisie", "...cuando se elige salud",
    "...quando si sceglie la salute", "...gdy wybrano zdrowie", "...když je zvoleno zdraví")
TRANSLATIONS["CPC_CW_PerMagicka"] = T(
    "...マジカを選んだとき", "...매지카를 선택했을 때", "...选择法力时", "...когда выбрана магия",
    "... wenn Magicka gewählt wird", "... quand la magie est choisie", "...cuando se elige magia",
    "...quando si sceglie la magicka", "...gdy wybrano magię", "...když je zvolena magicka")
TRANSLATIONS["CPC_CW_PerStamina"] = T(
    "...スタミナを選んだとき", "...스태미나를 선택했을 때", "...选择体力时", "...когда выбран запас сил",
    "... wenn Ausdauer gewählt wird", "... quand la vigueur est choisie", "...cuando se elige aguante",
    "...quando si sceglie il vigore", "...gdy wybrano kondycję", "...když je zvolena výdrž")
TRANSLATIONS["CPC_CW_HelpCross"] = T(
    "交差項です。バニラはスタミナで所持重量 5、他の 2 つでは 0 を与えます。",
    "교차 항목입니다. 바닐라는 스태미나에 소지 중량 5를 주고 나머지 둘에는 주지 않습니다.",
    "交叉项。原版在选择体力时给 5 点负重，另外两项不给。",
    "Перекрёстные значения. В оригинале запас сил даёт 5 переносимого веса, а два других - ничего.",
    "Die Querbezüge. Original gibt Ausdauer 5 Tragkraft und die anderen beiden nichts.",
    "Les termes croisés. Le jeu de base donne 5 de charge avec la vigueur et rien avec les deux autres.",
    "Los términos cruzados. El juego base da 5 de carga con aguante y nada con los otros dos.",
    "I termini incrociati. Il gioco base dà 5 di carico con il vigore e nulla con gli altri due.",
    "Człony krzyżowe. Podstawowa gra daje 5 udźwigu przy kondycji i nic przy dwóch pozostałych.",
    "Křížové členy. Základní hra dává 5 nosnosti u výdrže a nic u zbylých dvou.")
TRANSLATIONS["CPC_CW_NeedsLevelUpControl"] = T(
    "Level Up タブの「レベルアップの報酬を制御する」がオンのときに適用されます。今はオフなので、スタミナはバニラの 5 を与えます。",
    "Level Up 탭의 \"레벨업 보상 제어\"가 켜져 있을 때 적용됩니다. 지금은 꺼져 있으므로 스태미나는 바닐라의 5를 줍니다.",
    "仅在 Level Up 标签页的“控制升级奖励”开启时生效。目前它是关闭的，因此体力给予原版的 5。",
    "Применяется, пока на вкладке Level Up включено \"Управлять наградой за уровень\". Сейчас выключено, поэтому запас сил даёт оригинальные 5.",
    "Gilt, solange auf der Registerkarte Level Up \"Belohnung eines Stufenaufstiegs steuern\" an ist. Es ist aus, also gibt Ausdauer die originalen 5.",
    "S'applique tant que « Contrôler ce qu'accorde un niveau » est activé dans l'onglet Level Up. C'est désactivé, donc la vigueur donne les 5 du jeu de base.",
    "Se aplica mientras \"Controlar lo que da subir de nivel\" esté activado en la pestaña Level Up. Está apagado, así que el aguante da los 5 originales.",
    "Si applica finché \"Controlla cosa concede un passaggio di livello\" è attivo nella scheda Level Up. È spento, quindi il vigore dà i 5 del gioco base.",
    "Działa, gdy na karcie Level Up włączone jest \"Kontroluj nagrody za awans\". Jest wyłączone, więc kondycja daje podstawowe 5.",
    "Platí, dokud je na kartě Level Up zapnuto \"Řídit odměny za postup na úroveň\". Je vypnuto, takže výdrž dává základních 5.")
TRANSLATIONS["CPC_CW_Zeroed"] = T(
    "ゼロにしています: 当方の所持重量 MOD のいずれかが読み込まれており、所持重量を管理しています。",
    "0으로 두었습니다: 우리 단독 소지 중량 모드 중 하나가 로드되어 소지 중량을 관리합니다.",
    "已置零：我们的某个独立负重模组已加载，并掌管负重。",
    "Обнулено: загружен один из наших отдельных модов на переносимый вес, и он владеет им.",
    "Auf null gesetzt: einer unserer eigenständigen Tragkraft-Mods ist geladen und besitzt die Tragkraft.",
    "Mis à zéro : l'un de nos mods de charge autonomes est chargé et gère la charge.",
    "Puesto a cero: uno de nuestros mods independientes de carga está cargado y controla la carga.",
    "Azzerato: una delle nostre mod di carico autonome è caricata e possiede il carico.",
    "Wyzerowane: wczytany jest jeden z naszych samodzielnych modów udźwigu i to on nim zarządza.",
    "Vynulováno: je načten jeden z našich samostatných modů na nosnost a ten ji vlastní.")
TRANSLATIONS["CPC_CW_FormulaLine"] = T(
    "レベル %u: %.0f + %.1f x %u = %.0f", "레벨 %u: %.0f + %.1f x %u = %.0f", "等级 %u：%.0f + %.1f x %u = %.0f",
    "Уровень %u: %.0f + %.1f x %u = %.0f", "Stufe %u: %.0f + %.1f x %u = %.0f",
    "Niveau %u : %.0f + %.1f x %u = %.0f", "Nivel %u: %.0f + %.1f x %u = %.0f",
    "Livello %u: %.0f + %.1f x %u = %.0f", "Poziom %u: %.0f + %.1f x %u = %.0f",
    "Úroveň %u: %.0f + %.1f x %u = %.0f")
TRANSLATIONS["CPC_CW_PermanentLine"] = T(
    "恒久的な所持重量 %.0f、一時効果込みで現在 %.0f、この MOD による純増 %.0f",
    "영구 소지 중량 %.0f, 일시 효과 포함 현재 %.0f, 이 모드가 더한 순증 %.0f",
    "永久负重 %.0f，含临时效果当前为 %.0f，本模组净增加 %.0f",
    "Постоянный переносимый вес %.0f, сейчас %.0f с временными эффектами; чистая прибавка мода %.0f",
    "Dauerhafte Tragkraft %.0f, jetzt %.0f mit zeitweiligen Effekten; netto von diesem Mod %.0f",
    "Charge permanente %.0f, actuellement %.0f avec les effets temporaires ; ajout net de ce mod %.0f",
    "Carga permanente %.0f, ahora %.0f con efectos temporales; añadido neto por este mod %.0f",
    "Carico permanente %.0f, ora %.0f con gli effetti temporanei; aggiunto netto da questa mod %.0f",
    "Trwały udźwig %.0f, teraz %.0f z efektami tymczasowymi; netto od tego moda %.0f",
    "Trvalá nosnost %.0f, nyní %.0f s dočasnými efekty; čistý přírůstek od modu %.0f")
TRANSLATIONS["CPC_CW_NotControlling"] = T(
    "所持重量を制御していません - 上の値はゲームが実際に使っているものです。",
    "소지 중량을 제어하고 있지 않습니다 - 위 값은 게임이 실제로 쓰는 값입니다.",
    "未控制负重 - 上面的数值就是游戏当前实际使用的。",
    "Переносимый вес не управляется - значения выше те, что использует игра.",
    "Die Tragkraft wird nicht gesteuert - die Werte oben sind die, die das Spiel verwendet.",
    "La charge n'est pas contrôlée - les valeurs ci-dessus sont celles qu'utilise le jeu.",
    "No se está controlando la carga: los valores de arriba son los que usa el juego.",
    "Il carico non è controllato - i valori sopra sono quelli che il gioco sta usando.",
    "Udźwig nie jest kontrolowany - powyższe wartości to te, których używa gra.",
    "Nosnost není řízena - hodnoty výše jsou ty, které hra používá.")
TRANSLATIONS["CPC_CW_HelpApply"] = T(
    "今すぐ再計算します。セーブ読み込み時、レベルアップごと、上の項目を変更したときにも自動で実行されます。",
    "지금 다시 계산합니다. 세이브를 불러올 때, 레벨업마다, 위 항목을 바꿀 때도 자동으로 실행됩니다.",
    "立即重新计算。读取存档时、每次升级时以及你更改上面任何内容时也会自动执行。",
    "Пересчитывает сейчас. Также срабатывает само при загрузке сохранения, при каждом повышении уровня и при изменении чего-либо выше.",
    "Berechnet jetzt neu. Läuft außerdem von selbst beim Laden eines Spielstands, bei jedem Stufenaufstieg und wenn du oben etwas änderst.",
    "Recalcule maintenant. S'exécute aussi tout seul au chargement d'une sauvegarde, à chaque passage de niveau et quand vous modifiez quelque chose ci-dessus.",
    "Recalcula ahora. También se ejecuta solo al cargar una partida, en cada subida de nivel y cuando cambias algo arriba.",
    "Ricalcola ora. Viene eseguito anche da solo al caricamento di un salvataggio, a ogni passaggio di livello e quando modifichi qualcosa sopra.",
    "Przelicza teraz. Uruchamia się też samo przy wczytaniu zapisu, przy każdym awansie i gdy zmienisz cokolwiek powyżej.",
    "Přepočítá nyní. Spustí se také samo při načtení pozice, při každém postupu a když nahoře něco změníš.")

# --- Difficulty ----------------------------------------------------------------------------------
TRANSLATIONS["CPC_Diff_Intro"] = T(
    "難易度ごとの Skyrim 自身の数値です: ダメージ倍率と、再生 - バニラでは難易度によって変わりませんが、このタブがそれを加えます。プレイ中の難易度が、ゲームがどの組を読むかを決めます。",
    "난이도별 Skyrim 자체 수치입니다: 피해 배율과 재생 - 바닐라는 난이도에 따라 재생을 전혀 바꾸지 않지만, 이 탭이 그것을 추가합니다. 플레이 중인 난이도가 게임이 어느 세트를 읽을지 결정합니다.",
    "Skyrim 自身按难度划分的数值：伤害倍率与回复 - 原版根本不会按难度改变回复，本标签页补上了这一点。你所游玩的难度决定游戏读取哪一组数值。",
    "Собственные числа Skyrim по сложностям: множители урона и регенерация - которую оригинал вообще не меняет по сложности; эта вкладка это добавляет. Сложность, на которой вы играете, решает, какой набор читает игра.",
    "Skyrims eigene Zahlen je Schwierigkeitsgrad: die Schadensmultiplikatoren und die Regeneration - die das Original überhaupt nicht nach Schwierigkeitsgrad variiert; diese Registerkarte fügt das hinzu. Der Grad, auf dem du spielst, entscheidet, welchen Satz das Spiel liest.",
    "Les chiffres propres à Skyrim par difficulté : les multiplicateurs de dégâts et la régénération - que le jeu de base ne fait pas du tout varier selon la difficulté ; cet onglet l'ajoute. La difficulté à laquelle vous jouez décide du jeu de valeurs lu.",
    "Los números propios de Skyrim por dificultad: los multiplicadores de daño y la regeneración, que el juego base no varía en absoluto según la dificultad; esta pestaña lo añade. La dificultad en la que juegas decide qué conjunto lee el juego.",
    "I numeri propri di Skyrim per difficoltà: i moltiplicatori di danno e la rigenerazione - che il gioco base non varia affatto per difficoltà; questa scheda lo aggiunge. La difficoltà a cui giochi decide quale set legge il gioco.",
    "Własne liczby Skyrima na poziom trudności: mnożniki obrażeń i regeneracja - której podstawowa gra w ogóle nie różnicuje według trudności; ta karta to dodaje. Poziom trudności, na którym grasz, decyduje, który zestaw czyta gra.",
    "Vlastní čísla Skyrimu podle obtížnosti: násobitele poškození a regenerace - kterou základní hra podle obtížnosti vůbec nemění; tato karta to přidává. Obtížnost, na které hraješ, rozhoduje, kterou sadu hra čte.")
TRANSLATIONS["CPC_Diff_StandingDown"] = T(
    "待機中: Custom Difficulty UI が読み込まれており、これらの値を管理しています。その間、ここの設定は適用されません。",
    "대기 중: Custom Difficulty UI가 로드되어 이 값들을 관리합니다. 그동안 여기의 설정은 적용되지 않습니다.",
    "已让位：Custom Difficulty UI 已加载并掌管这些数值。在此期间此处的设置不会生效。",
    "Уступаем: загружен Custom Difficulty UI, и он владеет этими значениями. Пока это так, здесь ничего не применяется.",
    "Zurückgestellt: Custom Difficulty UI ist geladen und besitzt diese Werte. Solange das gilt, wird hier nichts angewendet.",
    "En retrait : Custom Difficulty UI est chargé et gère ces valeurs. Tant que c'est le cas, rien n'est appliqué ici.",
    "Cediendo el paso: Custom Difficulty UI está cargado y controla estos valores. Mientras sea así, aquí no se aplica nada.",
    "In disparte: Custom Difficulty UI è caricata e possiede questi valori. Finché è così, qui non viene applicato nulla.",
    "Ustępujemy: wczytany jest Custom Difficulty UI i to on zarządza tymi wartościami. Dopóki tak jest, nic tutaj nie jest stosowane.",
    "Ustupujeme: je načteno Custom Difficulty UI a to vlastní tyto hodnoty. Dokud to platí, zde se nic nepoužije.")
TRANSLATIONS["CPC_Diff_Now"] = T(
    "現在のゲーム難易度: %s", "현재 게임 난이도: %s", "当前游戏难度：%s", "Сложность игры сейчас: %s",
    "Spielschwierigkeit jetzt: %s", "Difficulté actuelle du jeu : %s", "Dificultad del juego ahora: %s",
    "Difficoltà di gioco attuale: %s", "Poziom trudności gry teraz: %s", "Obtížnost hry nyní: %s")
TRANSLATIONS["CPC_Diff_NoneLoaded"] = T(
    "なし (キャラクター未読み込み)", "없음 (캐릭터 미로드)", "无（未载入角色）",
    "нет (персонаж не загружен)", "keiner (kein Charakter geladen)", "aucune (aucun personnage chargé)",
    "ninguna (sin personaje cargado)", "nessuna (nessun personaggio caricato)",
    "brak (nie wczytano postaci)", "žádná (není načtena postava)")
TRANSLATIONS["CPC_Diff_OverhaulLoaded"] = T(
    "%s が読み込まれています。そのダメージ倍率と再生値が、下に表示されている読み込み時の値です。ここの制御をオンにするまで何も触れません - オンにすると、このタブが最後に書き込み、それらを上書きします。",
    "%s이(가) 로드되어 있습니다. 그 피해 배율과 재생 값이 아래에 표시된 로드 시 값입니다. 여기의 제어를 켜기 전에는 아무것도 건드리지 않으며, 켜면 이 탭이 마지막에 기록되어 그 값을 대체합니다.",
    "已加载 %s。它的伤害倍率与回复数值就是下面显示的载入值。在此处开启任一控制之前不会去动它们 - 一旦开启，本标签页最后写入并覆盖它们。",
    "Загружен %s. Его множители урона и значения регенерации - это загруженные значения, показанные ниже; здесь их ничто не трогает, пока не включён какой-нибудь контроль - тогда эта вкладка пишет последней и заменяет их.",
    "%s ist geladen. Seine Schadensmultiplikatoren und Regenerationswerte sind die unten gezeigten geladenen Werte; hier rührt nichts daran, bis ein Regler eingeschaltet wird - dann schreibt diese Registerkarte zuletzt und überschreibt sie.",
    "%s est chargé. Ses multiplicateurs de dégâts et ses valeurs de régénération sont les valeurs chargées affichées ci-dessous ; rien ici n'y touche tant qu'un contrôle n'est pas activé - alors cet onglet écrit en dernier et les remplace.",
    "%s está cargado. Sus multiplicadores de daño y valores de regeneración son los valores cargados que se muestran abajo; aquí nada los toca hasta que se active un control, y entonces esta pestaña escribe la última y los sustituye.",
    "%s è caricata. I suoi moltiplicatori di danno e i valori di rigenerazione sono i valori caricati mostrati sotto; qui nulla li tocca finché non si attiva un controllo - allora questa scheda scrive per ultima e li sostituisce.",
    "Wczytano %s. Jego mnożniki obrażeń i wartości regeneracji to pokazane niżej wartości wczytane; nic tutaj ich nie rusza, dopóki nie włączysz jakiejś kontrolki - wtedy ta karta zapisuje jako ostatnia i je zastępuje.",
    "Je načten %s. Jeho násobitele poškození a hodnoty regenerace jsou načtené hodnoty zobrazené níže; nic zde se jich nedotkne, dokud se nezapne některé řízení - pak tato karta zapisuje poslední a nahradí je.")
TRANSLATIONS["CPC_Diff_BBScaling"] = T(
    "BladeAndBlunt.ini で bLevelBasedDifficulty = true になっています: その DLL はレベル 10 から 50 でも倍率を段階的に変えます。このタブがダメージを制御している間は false にしてください - 1 つの値に 2 つの書き手がいる状態は安定しません。下の「レベルによる難易度」ルールが同じ仕事をします。",
    "BladeAndBlunt.ini에 bLevelBasedDifficulty = true 로 되어 있습니다: 그 DLL은 레벨 10에서 50 사이에서도 배율을 단계적으로 바꿉니다. 이 탭이 피해를 제어하는 동안에는 false로 두세요 - 하나의 값에 두 명의 기록자가 있으면 결코 안정적이지 않습니다. 아래의 레벨별 난이도 규칙이 같은 일을 합니다.",
    "BladeAndBlunt.ini 中 bLevelBasedDifficulty = true：它的 DLL 还会在 10 到 50 级之间分段调整倍率。当本标签页控制伤害时请将其设为 false - 一个数值有两个写入方永远不会稳定。下面的“按等级设定难度”规则做的是同一件事。",
    "В BladeAndBlunt.ini стоит bLevelBasedDifficulty = true: его DLL ещё и ступенчато меняет множители на уровнях с 10 по 50. Поставьте false, пока этой вкладкой управляется урон - два писателя на одно значение никогда не бывают стабильны. Правило \"сложность по уровню\" ниже делает ту же работу.",
    "In BladeAndBlunt.ini steht bLevelBasedDifficulty = true: seine DLL staffelt die Multiplikatoren zusätzlich zwischen Stufe 10 und 50. Setze es auf false, solange diese Registerkarte den Schaden steuert - zwei Schreiber auf einem Wert sind nie stabil. Die Regel \"Schwierigkeit nach Stufe\" unten erledigt dasselbe.",
    "BladeAndBlunt.ini contient bLevelBasedDifficulty = true : sa DLL fait aussi varier les multiplicateurs par paliers des niveaux 10 à 50. Mettez-le à false tant que cet onglet contrôle les dégâts - deux écrivains sur une même valeur ne sont jamais stables. La règle « Difficulté selon le niveau » ci-dessous fait le même travail.",
    "BladeAndBlunt.ini tiene bLevelBasedDifficulty = true: su DLL también escalona los multiplicadores entre los niveles 10 y 50. Ponlo en false mientras esta pestaña controle el daño: dos escritores sobre un mismo valor nunca son estables. La regla \"Dificultad por nivel\" de abajo hace ese mismo trabajo.",
    "BladeAndBlunt.ini ha bLevelBasedDifficulty = true: la sua DLL scala i moltiplicatori anche fra i livelli 10 e 50. Impostalo su false finché questa scheda controlla i danni - due scrittori su un solo valore non sono mai stabili. La regola \"Difficoltà per livello\" qui sotto fa lo stesso lavoro.",
    "W BladeAndBlunt.ini jest bLevelBasedDifficulty = true: jego DLL dodatkowo stopniuje mnożniki na poziomach od 10 do 50. Ustaw na false, dopóki ta karta kontroluje obrażenia - dwóch piszących do jednej wartości nigdy nie jest stabilne. Reguła \"Trudność według poziomu\" poniżej robi to samo.",
    "V BladeAndBlunt.ini je bLevelBasedDifficulty = true: jeho DLL navíc stupňuje násobitele mezi úrovněmi 10 a 50. Nastav ho na false, dokud tato karta řídí poškození - dva zapisovatelé na jednu hodnotu nejsou nikdy stabilní. Pravidlo \"Obtížnost podle úrovně\" níže dělá totéž.")
TRANSLATIONS["CPC_Diff_BBNotFound"] = T(
    "BladeAndBlunt.ini が見つからなかったため、その bLevelBasedDifficulty を読み取れませんでした。true になっている場合は、このタブがダメージを制御している間 false にしてください。",
    "BladeAndBlunt.ini을 찾지 못해 bLevelBasedDifficulty 값을 읽을 수 없었습니다. true라면 이 탭이 피해를 제어하는 동안 false로 두세요.",
    "未找到 BladeAndBlunt.ini，因此无法读取其 bLevelBasedDifficulty。如果它是 true，请在本标签页控制伤害时将其设为 false。",
    "BladeAndBlunt.ini не найден, поэтому его bLevelBasedDifficulty прочитать не удалось. Если он true, поставьте false, пока этой вкладкой управляется урон.",
    "BladeAndBlunt.ini wurde nicht gefunden, deshalb konnte bLevelBasedDifficulty nicht gelesen werden. Falls es true ist, setze es auf false, solange diese Registerkarte den Schaden steuert.",
    "BladeAndBlunt.ini est introuvable ; son bLevelBasedDifficulty n'a donc pas pu être lu. S'il vaut true, mettez-le à false tant que cet onglet contrôle les dégâts.",
    "No se encontró BladeAndBlunt.ini, así que no se pudo leer su bLevelBasedDifficulty. Si es true, ponlo en false mientras esta pestaña controle el daño.",
    "BladeAndBlunt.ini non è stato trovato, quindi il suo bLevelBasedDifficulty non ha potuto essere letto. Se è true, impostalo su false finché questa scheda controlla i danni.",
    "Nie znaleziono BladeAndBlunt.ini, więc nie udało się odczytać jego bLevelBasedDifficulty. Jeśli jest true, ustaw na false, dopóki ta karta kontroluje obrażenia.",
    "BladeAndBlunt.ini nebyl nalezen, takže jeho bLevelBasedDifficulty nešlo přečíst. Pokud je true, nastav ho na false, dokud tato karta řídí poškození.")
TRANSLATIONS["CPC_Diff_SecDamage"] = T(
    "ダメージ", "피해", "伤害", "Урон", "Schaden", "Dégâts", "Daño", "Danni", "Obrażenia", "Poškození")
TRANSLATIONS["CPC_Diff_ControlDamage"] = T(
    "ダメージ倍率を制御する", "피해 배율 제어", "控制伤害倍率", "Управлять множителями урона",
    "Schadensmultiplikatoren steuern", "Contrôler les multiplicateurs de dégâts",
    "Controlar los multiplicadores de daño", "Controlla i moltiplicatori di danno",
    "Kontroluj mnożniki obrażeń", "Řídit násobitele poškození")
TRANSLATIONS["CPC_Diff_HelpDamage"] = T(
    "オフでは何も書き込みません: ゲームが読み込んだ倍率 (バニラ、またはオーバーホールのもの) はそのまま残り、オフに戻せば返却されます。オンにすると下の組が書き込まれ、ゲームはプレイ中の難易度の組を読みます。",
    "꺼져 있으면 아무것도 기록하지 않습니다: 게임이 로드한 배율(바닐라 또는 오버홀의 값)이 그대로 남고, 끄면 그대로 돌려줍니다. 켜면 아래의 쌍이 기록되고, 게임은 플레이 중인 난이도의 쌍을 읽습니다.",
    "关闭时不写入任何内容：游戏载入时的倍率（原版或某个大修的）保持原样，关闭时也会交还。开启后会写入下面的数值对，游戏读取你所游玩难度对应的那一对。",
    "Выключено - ничего не пишется: множители, с которыми загрузилась игра (оригинальные или от оверхола), остаются как есть, а выключение возвращает их. Включено - пары ниже записываются, и игра читает пару для сложности, на которой вы играете.",
    "Aus schreibt nichts: die Multiplikatoren, mit denen dein Spiel geladen hat (original oder von einem Overhaul), bleiben genau so, und das Ausschalten gibt sie zurück. An werden die Paare unten geschrieben, und das Spiel liest das Paar für den Grad, auf dem du spielst.",
    "Désactivé n'écrit rien : les multiplicateurs chargés par votre jeu (de base ou d'une refonte) restent tels quels, et la désactivation les rend. Activé, les paires ci-dessous sont écrites et le jeu lit la paire de la difficulté à laquelle vous jouez.",
    "Apagado no escribe nada: los multiplicadores con los que cargó tu juego (los originales o los de una revisión) se quedan igual, y al apagarlo se devuelven. Encendido, se escriben los pares de abajo y el juego lee el par de la dificultad en la que juegas.",
    "Spento non scrive nulla: i moltiplicatori con cui il gioco si è caricato (base o di un overhaul) restano identici, e spegnendolo tornano. Acceso, le coppie sotto vengono scritte e il gioco legge la coppia della difficoltà a cui giochi.",
    "Wyłączone nie zapisuje nic: mnożniki, z którymi gra się wczytała (podstawowe lub z overhaulu), zostają bez zmian, a wyłączenie je oddaje. Włączone - pary poniżej są zapisywane, a gra czyta parę dla poziomu trudności, na którym grasz.",
    "Vypnuto nic nezapisuje: násobitele, se kterými se hra načetla (základní nebo z overhaulu), zůstanou přesně tak, a vypnutí je vrátí. Zapnuto se dvojice níže zapíšou a hra čte dvojici pro obtížnost, na které hraješ.")
TRANSLATIONS["CPC_Diff_SharedPair"] = T(
    "すべての難易度で 1 組を使う", "모든 난이도에 한 쌍 사용", "所有难度共用一对数值",
    "Одна пара для всех сложностей", "Ein Paar für jeden Schwierigkeitsgrad",
    "Une seule paire pour toutes les difficultés", "Un solo par para todas las dificultades",
    "Una coppia per ogni difficoltà", "Jedna para dla każdego poziomu trudności",
    "Jedna dvojice pro každou obtížnost")
TRANSLATIONS["CPC_Diff_HelpSharedPair"] = T(
    "オン: 下の 1 組が 6 段階すべてに書き込まれるため、ゲームの難易度設定はダメージに影響しなくなります。オフ: 難易度ごとに独自の組を持ちます。",
    "켬: 아래의 한 쌍이 여섯 난이도 모두에 기록되어 게임의 난이도 설정이 피해에 영향을 주지 않습니다. 끔: 난이도마다 자체 쌍을 가집니다.",
    "开启：下面这一对会写入全部六个难度，因此游戏的难度设置对伤害没有影响。关闭：每个难度各有一对。",
    "Включено: одна пара ниже пишется для всех шести сложностей, так что настройка сложности не влияет на урон. Выключено: у каждой сложности своя пара.",
    "An: das eine Paar unten wird für alle sechs Grade geschrieben, sodass die Schwierigkeitseinstellung keinen Unterschied beim Schaden macht. Aus: jeder Grad hat sein eigenes Paar.",
    "Activé : la paire unique ci-dessous est écrite pour les six difficultés, si bien que le réglage de difficulté ne change rien aux dégâts. Désactivé : chaque difficulté a sa propre paire.",
    "Activado: el único par de abajo se escribe para las seis dificultades, así que el ajuste de dificultad no cambia el daño. Desactivado: cada dificultad tiene su propio par.",
    "Attivo: l'unica coppia sotto viene scritta per tutte e sei le difficoltà, così l'impostazione di difficoltà non cambia i danni. Disattivo: ogni difficoltà ha la sua coppia.",
    "Włączone: jedna para poniżej jest zapisywana dla wszystkich sześciu poziomów, więc ustawienie trudności nie wpływa na obrażenia. Wyłączone: każdy poziom ma własną parę.",
    "Zapnuto: jediná dvojice níže se zapíše pro všech šest obtížností, takže nastavení obtížnosti na poškození nic nemění. Vypnuto: každá obtížnost má vlastní dvojici.")
TRANSLATIONS["CPC_Diff_DamageToYou"] = T(
    "自分が受けるダメージ", "내가 받는 피해", "你受到的伤害", "Урон по вам", "Schaden an dir",
    "Dégâts subis", "Daño que recibes", "Danni subiti", "Obrażenia otrzymywane", "Poškození tobě")
TRANSLATIONS["CPC_Diff_DamageByYou"] = T(
    "自分が与えるダメージ", "내가 주는 피해", "你造成的伤害", "Урон от вас", "Schaden durch dich",
    "Dégâts infligés", "Daño que causas", "Danni inflitti", "Obrażenia zadawane", "Poškození tebou")
TRANSLATIONS["CPC_Diff_HelpCtrlClick"] = T(
    "Ctrl キーを押しながらスライダーをクリックすると数値を入力できます。",
    "Ctrl 키를 누른 채 슬라이더를 클릭하면 값을 입력할 수 있습니다.",
    "按住 Ctrl 点击滑块可直接输入数值。",
    "Ctrl+щелчок по ползунку позволяет ввести значение.",
    "Strg+Klick auf einen Regler, um einen Wert einzutippen.",
    "Ctrl+clic sur un curseur pour saisir une valeur.",
    "Ctrl+clic en un deslizador para escribir un valor.",
    "Ctrl+clic su un cursore per digitare un valore.",
    "Ctrl+kliknięcie na suwaku pozwala wpisać wartość.",
    "Ctrl+klik na posuvník umožní hodnotu napsat.")
TRANSLATIONS["CPC_Diff_FillFrom"] = T(
    "表を次から埋める:", "표를 다음에서 채우기:", "从以下来源填充表格：", "Заполнить таблицу из:",
    "Tabelle füllen aus:", "Remplir la table depuis :", "Rellenar la tabla desde:",
    "Riempi la tabella da:", "Wypełnij tabelę z:", "Naplnit tabulku z:")
TRANSLATIONS["CPC_Diff_LoadedValues"] = T(
    "読み込み時の値", "로드된 값", "载入时的数值", "Загруженные значения", "Geladene Werte",
    "Valeurs chargées", "Valores cargados", "Valori caricati", "Wartości wczytane", "Načtené hodnoty")
TRANSLATIONS["CPC_Diff_StatusLoaded"] = T(
    "表にはこのゲームが読み込んだ値が入りました。",
    "표에 이 게임이 로드한 값이 들어갔습니다.",
    "表格中现在是本次游戏载入时的数值。",
    "В таблице значения, с которыми загрузилась эта игра.",
    "Die Tabelle enthält die Werte, mit denen dieses Spiel geladen hat.",
    "La table contient les valeurs avec lesquelles ce jeu a été chargé.",
    "La tabla contiene los valores con los que cargó este juego.",
    "La tabella contiene i valori con cui questo gioco si è caricato.",
    "Tabela zawiera wartości, z którymi wczytała się ta gra.",
    "Tabulka obsahuje hodnoty, se kterými se tato hra načetla.")
TRANSLATIONS["CPC_Diff_HelpLoadedValues"] = T(
    "ゲームが読み込み時点で持っている値です - バニラ、または導入しているオーバーホールのもの。オーバーホールの数値を失わずに調整を始めるための出発点です。",
    "게임이 로드 시점에 가지고 있는 값입니다 - 바닐라이거나, 사용 중인 오버홀의 값입니다. 오버홀의 수치를 잃지 않고 조정을 시작하기 좋은 출발점입니다.",
    "游戏载入时持有的数值 - 原版的，或你所用大修的。这是在不丢失大修数值的前提下开始调整的起点。",
    "То, что игра держит при загрузке - оригинал или ваш оверхол. Отправная точка для настройки оверхола без потери его чисел.",
    "Was dein Spiel beim Laden hält - original oder von dem Overhaul, den du fährst. Der Startpunkt, um einen Overhaul zu justieren, ohne seine Zahlen zu verlieren.",
    "Ce que votre jeu contient au chargement - le jeu de base, ou la refonte que vous utilisez. Le point de départ pour ajuster une refonte sans perdre ses chiffres.",
    "Lo que tu juego tiene al cargar: el original, o la revisión que uses. El punto de partida para ajustar una revisión sin perder sus números.",
    "Ciò che il tuo gioco ha al caricamento - base, o l'overhaul che usi. Il punto di partenza per regolare un overhaul senza perderne i numeri.",
    "To, co gra ma przy wczytaniu - podstawowe lub z overhaulu, którego używasz. Punkt wyjścia do strojenia overhaulu bez utraty jego liczb.",
    "To, co má tvá hra při načtení - základní, nebo z overhaulu, který používáš. Výchozí bod pro ladění overhaulu bez ztráty jeho čísel.")
TRANSLATIONS["CPC_Diff_StatusVanilla"] = T(
    "表には Skyrim のバニラ値が入りました。", "표에 Skyrim의 바닐라 값이 들어갔습니다.",
    "表格中现在是 Skyrim 的原版数值。", "В таблице ванильные значения Skyrim.",
    "Die Tabelle enthält Skyrims Originalwerte.", "La table contient les valeurs d'origine de Skyrim.",
    "La tabla contiene los valores originales de Skyrim.", "La tabella contiene i valori originali di Skyrim.",
    "Tabela zawiera podstawowe wartości Skyrima.", "Tabulka obsahuje původní hodnoty Skyrimu.")
TRANSLATIONS["CPC_Diff_StatusBladeAndBlunt"] = T(
    "表には Blade and Blunt の値が入りました。", "표에 Blade and Blunt의 값이 들어갔습니다.",
    "表格中现在是 Blade and Blunt 的数值。", "В таблице значения Blade and Blunt.",
    "Die Tabelle enthält die Werte von Blade and Blunt.", "La table contient les valeurs de Blade and Blunt.",
    "La tabla contiene los valores de Blade and Blunt.", "La tabella contiene i valori di Blade and Blunt.",
    "Tabela zawiera wartości Blade and Blunt.", "Tabulka obsahuje hodnoty Blade and Blunt.")
TRANSLATIONS["CPC_Diff_HelpBladeAndBlunt"] = T(
    "公表されている組み合わせです: 自分が受ける側はバニラどおり、与える側は 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5。",
    "공개된 쌍입니다: 받는 쪽은 바닐라와 같고, 주는 쪽은 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5 입니다.",
    "其公布的数值对：你受到的与原版相同，你造成的为 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5。",
    "Его опубликованные пары: по вам как в оригинале, от вас 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.",
    "Seine veröffentlichten Paare: an dir wie im Original, durch dich 1,5 / 1,25 / 1 / 1 / 0,75 / 0,5.",
    "Ses paires publiées : dégâts subis comme dans le jeu de base, dégâts infligés 1,5 / 1,25 / 1 / 1 / 0,75 / 0,5.",
    "Sus pares publicados: el daño que recibes como en el original, el que causas 1,5 / 1,25 / 1 / 1 / 0,75 / 0,5.",
    "Le sue coppie pubblicate: danni subiti come nel gioco base, danni inflitti 1,5 / 1,25 / 1 / 1 / 0,75 / 0,5.",
    "Jego opublikowane pary: obrażenia otrzymywane jak w podstawowej grze, zadawane 1,5 / 1,25 / 1 / 1 / 0,75 / 0,5.",
    "Jeho zveřejněné dvojice: poškození tobě jako v základní hře, tebou 1,5 / 1,25 / 1 / 1 / 0,75 / 0,5.")
TRANSLATIONS["CPC_Diff_StatusRequiem"] = T(
    "表には Requiem の値が入りました。", "표에 Requiem의 값이 들어갔습니다.",
    "表格中现在是 Requiem 的数值。", "В таблице значения Requiem.",
    "Die Tabelle enthält die Werte von Requiem.", "La table contient les valeurs de Requiem.",
    "La tabla contiene los valores de Requiem.", "La tabella contiene i valori di Requiem.",
    "Tabela zawiera wartości Requiem.", "Tabulka obsahuje hodnoty Requiem.")
TRANSLATIONS["CPC_Diff_HelpRequiem"] = T(
    "すべての倍率が 1.0 です - Requiem では設計上、難易度設定はダメージを一切スケールしません。",
    "모든 배율이 1.0입니다 - Requiem에서는 설계상 난이도 설정이 피해를 전혀 조정하지 않습니다.",
    "所有倍率均为 1.0 - 在 Requiem 中，难度设置按设计不对伤害做任何缩放。",
    "Все множители 1.0 - в Requiem настройка сложности по замыслу не масштабирует урон.",
    "Alle Multiplikatoren 1,0 - in Requiem skaliert die Schwierigkeitseinstellung den Schaden bewusst gar nicht.",
    "Tous les multiplicateurs à 1,0 - dans Requiem, le réglage de difficulté ne module pas les dégâts, par choix de conception.",
    "Todos los multiplicadores a 1,0: en Requiem el ajuste de dificultad no escala el daño, por diseño.",
    "Tutti i moltiplicatori a 1,0 - in Requiem l'impostazione di difficoltà non scala i danni, per scelta di progetto.",
    "Wszystkie mnożniki 1,0 - w Requiem ustawienie trudności z założenia nie skaluje obrażeń.",
    "Všechny násobitele 1,0 - v Requiem nastavení obtížnosti záměrně poškození vůbec neškáluje.")
TRANSLATIONS["CPC_Diff_Playing"] = T(
    "%s (プレイ中)", "%s (플레이 중)", "%s（当前游玩）", "%s (играете)", "%s (aktiv)",
    "%s (en cours)", "%s (en juego)", "%s (in corso)", "%s (grasz)", "%s (hraješ)")
TRANSLATIONS["CPC_Diff_LoadedWith"] = T(
    "    読み込み時: 受ける x%.2f、与える x%.2f",
    "    로드 시: 받는 피해 x%.2f, 주는 피해 x%.2f",
    "    载入时：受到 x%.2f，造成 x%.2f",
    "    загружено: x%.2f по вам, x%.2f от вас",
    "    geladen mit: x%.2f an dir, x%.2f durch dich",
    "    chargé avec : x%.2f subis, x%.2f infligés",
    "    cargado con: x%.2f recibido, x%.2f causado",
    "    caricato con: x%.2f subiti, x%.2f inflitti",
    "    wczytano z: x%.2f otrzymywane, x%.2f zadawane",
    "    načteno s: x%.2f tobě, x%.2f tebou")
TRANSLATIONS["CPC_Diff_HelpVanillaPairs"] = T(
    "バニラ: 受ける 0.5 / 0.75 / 1 / 1.5 / 2 / 3、与える 2 / 1.5 / 1 / 0.75 / 0.5 / 0.25 (初心者から伝説まで)。Ctrl キーを押しながらスライダーをクリックすると数値を入力できます。",
    "바닐라: 받는 피해 0.5 / 0.75 / 1 / 1.5 / 2 / 3, 주는 피해 2 / 1.5 / 1 / 0.75 / 0.5 / 0.25 (초보자부터 전설까지). Ctrl 키를 누른 채 슬라이더를 클릭하면 값을 입력할 수 있습니다.",
    "原版：受到 0.5 / 0.75 / 1 / 1.5 / 2 / 3，造成 2 / 1.5 / 1 / 0.75 / 0.5 / 0.25（从新手到传奇）。按住 Ctrl 点击滑块可直接输入数值。",
    "Оригинал: по вам 0.5 / 0.75 / 1 / 1.5 / 2 / 3, от вас 2 / 1.5 / 1 / 0.75 / 0.5 / 0.25, от Новичка до Легендарного. Ctrl+щелчок по ползунку позволяет ввести значение.",
    "Original: an dir 0,5 / 0,75 / 1 / 1,5 / 2 / 3, durch dich 2 / 1,5 / 1 / 0,75 / 0,5 / 0,25, von Novize bis Legendär. Strg+Klick auf einen Regler, um einen Wert einzutippen.",
    "Jeu de base : subis 0,5 / 0,75 / 1 / 1,5 / 2 / 3, infligés 2 / 1,5 / 1 / 0,75 / 0,5 / 0,25, de Novice à Légendaire. Ctrl+clic sur un curseur pour saisir une valeur.",
    "Juego base: recibido 0,5 / 0,75 / 1 / 1,5 / 2 / 3, causado 2 / 1,5 / 1 / 0,75 / 0,5 / 0,25, de Novato a Legendario. Ctrl+clic en un deslizador para escribir un valor.",
    "Gioco base: subiti 0,5 / 0,75 / 1 / 1,5 / 2 / 3, inflitti 2 / 1,5 / 1 / 0,75 / 0,5 / 0,25, da Novizio a Leggendario. Ctrl+clic su un cursore per digitare un valore.",
    "Podstawowa gra: otrzymywane 0,5 / 0,75 / 1 / 1,5 / 2 / 3, zadawane 2 / 1,5 / 1 / 0,75 / 0,5 / 0,25, od Nowicjusza do Legendarnego. Ctrl+kliknięcie na suwaku pozwala wpisać wartość.",
    "Základní hra: tobě 0,5 / 0,75 / 1 / 1,5 / 2 / 3, tebou 2 / 1,5 / 1 / 0,75 / 0,5 / 0,25, od Nováčka po Legendární. Ctrl+klik na posuvník umožní hodnotu napsat.")
TRANSLATIONS["CPC_Diff_SecByLevel"] = T(
    "レベルによる難易度", "레벨에 따른 난이도", "按等级设定难度", "Сложность по уровню",
    "Schwierigkeit nach Stufe", "Difficulté selon le niveau", "Dificultad por nivel",
    "Difficoltà per livello", "Trudność według poziomu", "Obtížnost podle úrovně")
TRANSLATIONS["CPC_Diff_ByLevel"] = T(
    "ゲームの難易度を自分のレベルから決める", "게임 난이도를 내 레벨에서 정하기", "根据你的等级设定游戏难度",
    "Задавать сложность игры по вашему уровню", "Die Spielschwierigkeit aus deiner Stufe setzen",
    "Définir la difficulté du jeu d'après votre niveau", "Fijar la dificultad del juego según tu nivel",
    "Imposta la difficoltà di gioco dal tuo livello", "Ustaw trudność gry na podstawie twojego poziomu",
    "Nastavit obtížnost hry podle tvé úrovně")
TRANSLATIONS["CPC_Diff_HelpByLevel"] = T(
    "セーブ読み込み時とレベルアップごとに、到達したレベルに対応する最も高い難易度がゲームの難易度になります - 設定メニューで変えるのと同じ変更なので、難易度に従うものはすべて追随します。0 = その難易度はこのルールで選ばれません。オフ: ゲームの難易度はあなたが決めます。",
    "세이브를 불러올 때와 레벨업할 때마다, 도달한 레벨에 해당하는 가장 높은 난이도가 게임 난이도가 됩니다 - 설정 메뉴에서 바꾸는 것과 같은 변경이므로 난이도를 따르는 모든 것이 따라옵니다. 0 = 이 규칙으로는 그 난이도가 선택되지 않습니다. 끔: 게임 난이도는 당신이 정합니다.",
    "在读取存档时以及每次升级时，你已达到其等级要求的最高难度会成为游戏难度 - 这与在设置菜单中更改是同一种改动，因此所有跟随难度的东西都会跟着变。0 = 该难度永远不会被此规则选中。关闭：游戏难度由你自己设定。",
    "При загрузке сохранения и при каждом повышении уровня сложностью игры становится самая высокая сложность, чей уровень вы достигли - то же изменение, что делает меню настроек, поэтому за ним следует всё, что следует за сложностью. 0 = эта сложность никогда не выбирается правилом. Выключено: сложность игры - ваше дело.",
    "Beim Laden eines Spielstands und bei jedem Stufenaufstieg wird der höchste Schwierigkeitsgrad, dessen Stufe du erreicht hast, zur Spielschwierigkeit - dieselbe Änderung, die das Einstellungsmenü macht, also folgt alles mit, was der Schwierigkeit folgt. 0 = dieser Grad wird von dieser Regel nie gewählt. Aus: die Spielschwierigkeit gehört dir.",
    "Au chargement d'une sauvegarde et à chaque passage de niveau, la difficulté la plus élevée dont vous avez atteint le niveau devient la difficulté du jeu - c'est le même changement que celui du menu Paramètres, donc tout ce qui suit la difficulté suit. 0 = cette difficulté n'est jamais choisie par cette règle. Désactivé : la difficulté du jeu vous appartient.",
    "Al cargar una partida y en cada subida de nivel, la dificultad más alta cuyo nivel hayas alcanzado pasa a ser la dificultad del juego: es el mismo cambio que hace el menú de opciones, así que todo lo que sigue a la dificultad la sigue. 0 = esa dificultad nunca la elige esta regla. Desactivado: la dificultad del juego es cosa tuya.",
    "Al caricamento di un salvataggio e a ogni passaggio di livello, la difficoltà più alta il cui livello hai raggiunto diventa la difficoltà di gioco - è lo stesso cambiamento del menu Impostazioni, quindi tutto ciò che segue la difficoltà la segue. 0 = quella difficoltà non viene mai scelta da questa regola. Disattivo: la difficoltà di gioco è tua.",
    "Przy wczytaniu zapisu i przy każdym awansie najwyższy poziom trudności, którego poziom osiągnąłeś, staje się trudnością gry - to ta sama zmiana, którą robi menu ustawień, więc wszystko, co podąża za trudnością, podąża i tu. 0 = ta trudność nigdy nie jest wybierana przez tę regułę. Wyłączone: trudność gry należy do ciebie.",
    "Při načtení pozice a při každém postupu se obtížností hry stane nejvyšší obtížnost, jejíž úrovně jsi dosáhl - je to stejná změna, jakou dělá nabídka nastavení, takže vše, co obtížnost sleduje, ji sleduje. 0 = tuto obtížnost pravidlo nikdy nezvolí. Vypnuto: obtížnost hry je na tobě.")
TRANSLATIONS["CPC_Diff_LevelMapsTo"] = T(
    "レベル %d -> %s", "레벨 %d -> %s", "等级 %d -> %s", "Уровень %d -> %s", "Stufe %d -> %s",
    "Niveau %d -> %s", "Nivel %d -> %s", "Livello %d -> %s", "Poziom %d -> %s", "Úroveň %d -> %s")
TRANSLATIONS["CPC_Diff_NoRowApplies"] = T(
    "該当する行なし", "해당하는 행 없음", "无适用行", "нет подходящей строки",
    "keine Zeile trifft zu", "aucune ligne ne s'applique", "no se aplica ninguna fila",
    "nessuna riga si applica", "żaden wiersz nie pasuje", "žádný řádek neplatí")
TRANSLATIONS["CPC_Diff_FromLevel"] = T(
    "%s はこのレベルから", "%s 는 이 레벨부터", "%s 起始等级", "%s с уровня", "%s ab Stufe",
    "%s à partir du niveau", "%s desde el nivel", "%s dal livello", "%s od poziomu", "%s od úrovně")
TRANSLATIONS["CPC_Diff_HelpMilestones"] = T(
    "既定値は Blade and Blunt の区切りです: 10 レベルごとに難易度が 1 段階上がります。",
    "기본값은 Blade and Blunt의 기준점입니다: 10레벨마다 난이도 한 단계입니다.",
    "默认值取自 Blade and Blunt 的里程碑：每十级提升一个难度档。",
    "По умолчанию - вехи Blade and Blunt: одна ступень сложности на каждые десять уровней.",
    "Standard sind die Meilensteine von Blade and Blunt: eine Schwierigkeitsstufe je zehn Stufen.",
    "Les valeurs par défaut sont les jalons de Blade and Blunt : un palier de difficulté tous les dix niveaux.",
    "Los valores predeterminados son los hitos de Blade and Blunt: un escalón de dificultad cada diez niveles.",
    "I valori predefiniti sono le tappe di Blade and Blunt: un gradino di difficoltà ogni dieci livelli.",
    "Domyślne to kamienie milowe Blade and Blunt: jeden stopień trudności co dziesięć poziomów.",
    "Výchozí jsou milníky Blade and Blunt: jeden stupeň obtížnosti na deset úrovní.")
TRANSLATIONS["CPC_Diff_SecRegen"] = T(
    "再生", "재생", "回复", "Регенерация", "Regeneration", "Régénération", "Regeneración",
    "Rigenerazione", "Regeneracja", "Regenerace")
TRANSLATIONS["CPC_Diff_ControlRegen"] = T(
    "再生を制御する", "재생 제어", "控制回复", "Управлять регенерацией", "Regeneration steuern",
    "Contrôler la régénération", "Controlar la regeneración", "Controlla la rigenerazione",
    "Kontroluj regenerację", "Řídit regeneraci")
TRANSLATIONS["CPC_Diff_HelpRegen"] = T(
    "バニラでは再生値は全難易度で 1 組だけです。ここでは難易度ごとに独自の組を持ち、プレイ中の難易度の組をゲームが読みます - ゲームの設定で難易度を変えた瞬間に切り替わります。オフにすると、ゲームが元々持っていた値に戻します。",
    "바닐라에서는 재생 값이 모든 난이도에 대해 한 세트뿐입니다. 여기서는 난이도마다 자체 세트를 가지며, 플레이 중인 난이도의 세트를 게임이 읽습니다 - 게임 설정에서 난이도를 바꾸는 순간 전환됩니다. 끄면 게임이 원래 가지고 있던 값으로 되돌립니다.",
    "原版所有难度共用一套回复数值。这里每个难度各有一套，游戏读取的是你所游玩难度的那一套 - 在游戏设置中更改难度的瞬间即切换。关闭后会还原为游戏原本的数值。",
    "В оригинале один набор значений регенерации на все сложности. Здесь у каждой сложности свой набор, и игра читает набор той сложности, на которой вы играете - переключается в тот момент, когда вы меняете сложность в настройках игры. Выключение возвращает значения, с которыми пришла игра.",
    "Im Original gibt es einen Satz Regenerationswerte für alle Schwierigkeitsgrade. Hier hat jeder Grad seinen eigenen Satz, und das Spiel liest den Satz des Grades, auf dem du spielst - umgeschaltet in dem Moment, in dem du die Schwierigkeit in den Spieleinstellungen änderst. Aus stellt die Werte wieder her, mit denen dein Spiel kam.",
    "Le jeu de base n'a qu'un seul jeu de valeurs de régénération pour toutes les difficultés. Ici chaque difficulté a le sien, et le jeu lit celui de la difficulté à laquelle vous jouez - basculé dès que vous changez la difficulté dans les paramètres du jeu. Désactiver restaure les valeurs d'origine de votre jeu.",
    "El juego base tiene un único conjunto de valores de regeneración para todas las dificultades. Aquí cada dificultad tiene el suyo, y el juego lee el de la dificultad en la que juegas: cambia en el momento en que cambias la dificultad en los ajustes del juego. Apagarlo restaura los valores con los que vino tu juego.",
    "Il gioco base ha un solo set di valori di rigenerazione per tutte le difficoltà. Qui ogni difficoltà ha il suo, e il gioco legge quello della difficoltà a cui giochi - cambiato nel momento in cui modifichi la difficoltà nelle impostazioni del gioco. Spegnere ripristina i valori con cui il gioco è arrivato.",
    "W podstawowej grze jest jeden zestaw wartości regeneracji dla wszystkich poziomów trudności. Tutaj każdy poziom ma własny, a gra czyta zestaw poziomu, na którym grasz - przełączany w chwili zmiany trudności w ustawieniach gry. Wyłączenie przywraca wartości, z którymi gra przyszła.",
    "V základní hře je jedna sada hodnot regenerace pro všechny obtížnosti. Zde má každá obtížnost vlastní a hra čte sadu obtížnosti, na které hraješ - přepne se ve chvíli, kdy obtížnost změníš v nastavení hry. Vypnutí obnoví hodnoty, se kterými hra přišla.")
TRANSLATIONS["CPC_Diff_Editing"] = T(
    "編集対象", "편집 대상", "正在编辑", "Редактируется", "Bearbeitet",
    "Édition", "Editando", "In modifica", "Edytowany", "Upravuje se")
TRANSLATIONS["CPC_Diff_HelpEditing"] = T(
    "下のスライダーがどの難易度の組を表示するかです。ゲームはプレイ中の難易度の組を読みます。",
    "아래 슬라이더가 어느 난이도의 세트를 보여줄지입니다. 게임은 플레이 중인 난이도의 세트를 읽습니다.",
    "下面的滑块显示的是哪个难度的数值组。游戏读取的是你所游玩难度的那一组。",
    "Набор какой сложности показывают ползунки ниже. Игра читает набор той сложности, на которой вы играете.",
    "Welchen Satz die Regler unten zeigen. Das Spiel liest den Satz des Grades, auf dem du spielst.",
    "Le jeu de valeurs affiché par les curseurs ci-dessous. Le jeu lit celui de la difficulté à laquelle vous jouez.",
    "Qué conjunto muestran los deslizadores de abajo. El juego lee el de la dificultad en la que juegas.",
    "Quale set mostrano i cursori qui sotto. Il gioco legge quello della difficoltà a cui giochi.",
    "Który zestaw pokazują suwaki poniżej. Gra czyta zestaw poziomu, na którym grasz.",
    "Kterou sadu ukazují posuvníky níže. Hra čte sadu obtížnosti, na které hraješ.")
TRANSLATIONS["CPC_Diff_CopyToAll"] = T(
    "すべての難易度にコピー", "모든 난이도에 복사", "复制到所有难度", "Скопировать во все сложности",
    "Auf alle Schwierigkeitsgrade kopieren", "Copier vers toutes les difficultés",
    "Copiar a todas las dificultades", "Copia in tutte le difficoltà",
    "Skopiuj do wszystkich poziomów", "Zkopírovat do všech obtížností")
TRANSLATIONS["CPC_Diff_HelpCopyToAll"] = T(
    "他のすべての難易度の組が、この組のコピーになります。",
    "다른 모든 난이도의 세트가 이 세트의 복사본이 됩니다.",
    "其他所有难度的数值组都会变成这一组的副本。",
    "Набор каждой другой сложности становится копией этого.",
    "Der Satz jedes anderen Schwierigkeitsgrades wird zu einer Kopie dieses Satzes.",
    "Le jeu de valeurs de chaque autre difficulté devient une copie de celui-ci.",
    "El conjunto de cada otra dificultad pasa a ser una copia de este.",
    "Il set di ogni altra difficoltà diventa una copia di questo.",
    "Zestaw każdego innego poziomu staje się kopią tego.",
    "Sada každé jiné obtížnosti se stane kopií této.")
TRANSLATIONS["CPC_Diff_NotInSettings"] = T(
    "%s - このゲームの設定には存在しません", "%s - 이 게임의 설정에 없습니다", "%s - 本作的设置中没有此项",
    "%s - нет в настройках этой игры", "%s - in den Einstellungen dieses Spiels nicht vorhanden",
    "%s - absent des paramètres de ce jeu", "%s: no está en los ajustes de este juego",
    "%s - non presente nelle impostazioni di questo gioco", "%s - brak w ustawieniach tej gry",
    "%s - není v nastavení této hry")
TRANSLATIONS["CPC_Diff_HelpRegenValue"] = T(
    "%s - このゲーム自身の値は %.2f です。", "%s - 이 게임 자체의 값은 %.2f 입니다.",
    "%s - 本作自身的数值为 %.2f。", "%s - собственное значение вашей игры %.2f.",
    "%s - der eigene Wert deines Spiels ist %.2f.", "%s - la valeur propre à votre jeu est %.2f.",
    "%s: el valor propio de tu juego es %.2f.", "%s - il valore proprio del tuo gioco è %.2f.",
    "%s - własna wartość twojej gry to %.2f.", "%s - vlastní hodnota tvé hry je %.2f.")
TRANSLATIONS["CPC_Diff_SecEveryDifficulty"] = T(
    "すべての難易度で共通", "모든 난이도 공통", "所有难度通用", "Для всех сложностей",
    "Für jeden Schwierigkeitsgrad", "Toutes difficultés", "Todas las dificultades",
    "Tutte le difficoltà", "Wszystkie poziomy trudności", "Všechny obtížnosti")
TRANSLATIONS["CPC_Diff_HelpGlobalValue"] = T(
    "%s - すべての難易度で 1 つの値です。このゲーム自身の値は %.2f です。",
    "%s - 모든 난이도에 하나의 값입니다. 이 게임 자체의 값은 %.2f 입니다.",
    "%s - 所有难度共用一个数值；本作自身的数值为 %.2f。",
    "%s - одно значение для всех сложностей; собственное значение вашей игры %.2f.",
    "%s - ein Wert für jeden Schwierigkeitsgrad; der eigene Wert deines Spiels ist %.2f.",
    "%s - une seule valeur pour toutes les difficultés ; la valeur propre à votre jeu est %.2f.",
    "%s: un solo valor para todas las dificultades; el valor propio de tu juego es %.2f.",
    "%s - un unico valore per tutte le difficoltà; il valore proprio del tuo gioco è %.2f.",
    "%s - jedna wartość dla wszystkich poziomów; własna wartość twojej gry to %.2f.",
    "%s - jedna hodnota pro všechny obtížnosti; vlastní hodnota tvé hry je %.2f.")
TRANSLATIONS["CPC_Diff_SecUsingNow"] = T(
    "ゲームが現在使っている値", "게임이 지금 쓰는 값", "游戏当前使用的数值",
    "Что игра использует сейчас", "Was das Spiel gerade verwendet", "Ce que le jeu utilise actuellement",
    "Lo que usa el juego ahora", "Cosa sta usando il gioco ora", "Czego gra używa teraz",
    "Co hra právě používá")
TRANSLATIONS["CPC_Diff_DamageLine"] = T(
    "%s でのダメージ: 受ける x%.2f、与える x%.2f (読み込み時 x%.2f / x%.2f)",
    "%s 에서의 피해: 받는 피해 x%.2f, 주는 피해 x%.2f (로드 시 x%.2f / x%.2f)",
    "%s 难度下的伤害：受到 x%.2f，造成 x%.2f（载入时 x%.2f / x%.2f）",
    "Урон на сложности %s: x%.2f по вам, x%.2f от вас (загружено с x%.2f / x%.2f)",
    "Schaden auf %s: x%.2f an dir, x%.2f durch dich (geladen mit x%.2f / x%.2f)",
    "Dégâts en %s : x%.2f subis, x%.2f infligés (chargé avec x%.2f / x%.2f)",
    "Daño en %s: x%.2f recibido, x%.2f causado (cargado con x%.2f / x%.2f)",
    "Danni a %s: x%.2f subiti, x%.2f inflitti (caricato con x%.2f / x%.2f)",
    "Obrażenia na %s: x%.2f otrzymywane, x%.2f zadawane (wczytano z x%.2f / x%.2f)",
    "Poškození na %s: x%.2f tobě, x%.2f tebou (načteno s x%.2f / x%.2f)")
TRANSLATIONS["CPC_Diff_RegenLine"] = T(
    "戦闘中の再生: 体力 x%.2f、マジカ x%.2f、スタミナ x%.2f",
    "전투 중 재생: 체력 x%.2f, 매지카 x%.2f, 스태미나 x%.2f",
    "战斗中回复：生命 x%.2f，法力 x%.2f，体力 x%.2f",
    "Регенерация в бою: здоровье x%.2f, магия x%.2f, запас сил x%.2f",
    "Regeneration im Kampf: Gesundheit x%.2f, Magicka x%.2f, Ausdauer x%.2f",
    "Régénération en combat : santé x%.2f, magie x%.2f, vigueur x%.2f",
    "Regeneración en combate: salud x%.2f, magia x%.2f, aguante x%.2f",
    "Rigenerazione in combattimento: salute x%.2f, magicka x%.2f, vigore x%.2f",
    "Regeneracja w walce: zdrowie x%.2f, magia x%.2f, kondycja x%.2f",
    "Regenerace v boji: zdraví x%.2f, magicka x%.2f, výdrž x%.2f")
TRANSLATIONS["CPC_Diff_DelayLine"] = T(
    "被弾後の遅延: 体力 %.1f 秒、マジカ %.1f 秒、スタミナ %.1f 秒",
    "피해 후 지연: 체력 %.1f 초, 매지카 %.1f 초, 스태미나 %.1f 초",
    "受伤后的延迟：生命 %.1f 秒，法力 %.1f 秒，体力 %.1f 秒",
    "Задержка после урона: здоровье %.1f с, магия %.1f с, запас сил %.1f с",
    "Verzögerung nach Schaden: Gesundheit %.1f s, Magicka %.1f s, Ausdauer %.1f s",
    "Délai après dégâts : santé %.1f s, magie %.1f s, vigueur %.1f s",
    "Retardo tras el daño: salud %.1f s, magia %.1f s, aguante %.1f s",
    "Ritardo dopo il danno: salute %.1f s, magicka %.1f s, vigore %.1f s",
    "Opóźnienie po obrażeniach: zdrowie %.1f s, magia %.1f s, kondycja %.1f s",
    "Zpoždění po poškození: zdraví %.1f s, magicka %.1f s, výdrž %.1f s")
TRANSLATIONS["CPC_Diff_NotControlling"] = T(
    "どちらも制御していません - 何も書き込まれておらず、上の値はゲームが読み込んだままのものです。",
    "둘 다 제어하고 있지 않습니다 - 아무것도 기록되지 않았고, 위 값은 게임이 로드한 그대로입니다.",
    "两者都未控制 - 未写入任何内容；上面的数值就是游戏载入时的样子。",
    "Ни то, ни другое не управляется - ничего не пишется; значения выше те, с которыми загрузилась игра.",
    "Weder noch wird gesteuert - es wird nichts geschrieben; die Werte oben sind die, mit denen das Spiel geladen hat.",
    "Ni l'un ni l'autre n'est contrôlé - rien n'est écrit ; les valeurs ci-dessus sont celles avec lesquelles le jeu a été chargé.",
    "No se controla ninguno de los dos: no se escribe nada; los valores de arriba son con los que cargó el juego.",
    "Nessuno dei due è controllato - non viene scritto nulla; i valori sopra sono quelli con cui il gioco si è caricato.",
    "Żadne z nich nie jest kontrolowane - nic nie jest zapisywane; powyższe wartości to te, z którymi gra się wczytała.",
    "Ani jedno není řízeno - nic se nezapisuje; hodnoty výše jsou ty, se kterými se hra načetla.")
TRANSLATIONS["CPC_Diff_HelpApply"] = T(
    "値を書き直します。セーブ読み込み時、ゲームの難易度が変わったとき、上の項目を変更したときにも自動で実行されます。",
    "값을 다시 씁니다. 세이브를 불러올 때, 게임 난이도가 바뀔 때, 위 항목을 바꿀 때도 자동으로 실행됩니다.",
    "重新写入这些数值。读取存档时、游戏难度改变时以及你更改上面任何内容时也会自动执行。",
    "Перезаписывает значения. Также срабатывает само при загрузке сохранения, при смене сложности игры и при изменении чего-либо выше.",
    "Schreibt die Werte neu. Läuft außerdem von selbst beim Laden eines Spielstands, wenn sich die Spielschwierigkeit ändert und wenn du oben etwas änderst.",
    "Réécrit les valeurs. S'exécute aussi tout seul au chargement d'une sauvegarde, quand la difficulté du jeu change et quand vous modifiez quelque chose ci-dessus.",
    "Reescribe los valores. También se ejecuta solo al cargar una partida, cuando cambia la dificultad del juego y cuando cambias algo arriba.",
    "Riscrive i valori. Viene eseguito anche da solo al caricamento di un salvataggio, quando cambia la difficoltà di gioco e quando modifichi qualcosa sopra.",
    "Ponownie zapisuje wartości. Uruchamia się też samo przy wczytaniu zapisu, przy zmianie trudności gry i gdy zmienisz cokolwiek powyżej.",
    "Znovu zapíše hodnoty. Spustí se také samo při načtení pozice, při změně obtížnosti hry a když nahoře něco změníš.")

# --- Experience ----------------------------------------------------------------------------------
TRANSLATIONS["CPC_Exp_Intro"] = T(
    "行動から得られるキャラクター経験値です: クエスト完了、場所の発見、ダンジョンの制圧、キル、読書 - スキル使用と併用することも、その代わりにすることもできます。各値はこの MOD 独自の数字です。Levelling タブのレベル費用と見比べて調整してください。",
    "행동으로 얻는 캐릭터 경험치입니다: 퀘스트 완료, 장소 발견, 던전 소탕, 처치, 독서 - 기술 사용과 함께 쓰거나 그 대신 쓸 수 있습니다. 각 수치는 이 모드 고유의 값입니다. Levelling 탭의 레벨 비용과 견주어 조정하세요.",
    "来自你所作所为的角色经验：完成任务、发现地点、清空地城、击杀与阅读 - 可与技能使用并行，也可取而代之。每个数额都是本模组自己的数字；请对照 Levelling 标签页的等级花费来调整。",
    "Опыт персонажа за то, что вы делаете: выполненные задания, найденные места, зачищенные подземелья, убийства и прочитанные книги - вместе с использованием навыков или вместо него. Каждая величина - собственное число этого мода; настраивайте её относительно стоимости уровня на вкладке Levelling.",
    "Charaktererfahrung aus dem, was du tust: abgeschlossene Quests, entdeckte Orte, geräumte Verliese, Tötungen und gelesene Bücher - neben der Fertigkeitsnutzung oder an ihrer Stelle. Jeder Betrag ist eine eigene Zahl dieses Mods; stimme sie auf die Stufenkosten der Registerkarte Levelling ab.",
    "L'expérience de personnage tirée de ce que vous faites : quêtes terminées, lieux découverts, donjons nettoyés, victimes et livres lus - en plus de l'usage des compétences, ou à sa place. Chaque montant est un chiffre propre à ce mod ; réglez-le par rapport au coût de niveau de l'onglet Levelling.",
    "Experiencia de personaje por lo que haces: misiones completadas, lugares descubiertos, mazmorras despejadas, muertes y libros leídos, junto al uso de habilidades o en su lugar. Cada cantidad es un número propio de este mod; ajústalo frente al coste de nivel de la pestaña Levelling.",
    "Esperienza del personaggio da ciò che fai: missioni completate, luoghi scoperti, sotterranei ripuliti, uccisioni e libri letti - accanto all'uso delle abilità o al suo posto. Ogni importo è un numero proprio di questa mod; regolalo rispetto al costo del livello nella scheda Levelling.",
    "Doświadczenie postaci za to, co robisz: ukończone zadania, odkryte miejsca, oczyszczone lochy, zabójstwa i przeczytane księgi - obok używania umiejętności albo zamiast niego. Każda kwota to własna liczba tego moda; dostrój ją względem kosztu poziomu z karty Levelling.",
    "Zkušenost postavy z toho, co děláš: dokončené úkoly, objevená místa, vyčištěná doupata, zabití a přečtené knihy - vedle používání dovedností nebo místo něj. Každá částka je vlastní číslo tohoto modu; nalaď ji proti ceně úrovně na kartě Levelling.")
TRANSLATIONS["CPC_Exp_StandingDown"] = T(
    "待機中: Experience が読み込まれており、経験値の入手元を管理しています。その間、ここからは何も与えられません。",
    "대기 중: Experience가 로드되어 경험치의 출처를 관리합니다. 그동안 여기서는 아무것도 주지 않습니다.",
    "已让位：Experience 已加载并掌管经验来源。在此期间此处不会给予任何经验。",
    "Уступаем: загружен мод Experience, и он владеет тем, откуда берётся опыт. Пока это так, отсюда ничего не начисляется.",
    "Zurückgestellt: der Mod Experience ist geladen und bestimmt, woher Erfahrung kommt. Solange das gilt, wird hier nichts vergeben.",
    "En retrait : le mod Experience est chargé et détermine d'où vient l'expérience. Tant que c'est le cas, rien n'est accordé ici.",
    "Cediendo el paso: el mod Experience está cargado y controla de dónde viene la experiencia. Mientras sea así, aquí no se otorga nada.",
    "In disparte: la mod Experience è caricata e governa da dove arriva l'esperienza. Finché è così, qui non viene concesso nulla.",
    "Ustępujemy: wczytany jest mod Experience i to on decyduje, skąd bierze się doświadczenie. Dopóki tak jest, nic tutaj nie jest przyznawane.",
    "Ustupujeme: je načten mod Experience a ten určuje, odkud se bere zkušenost. Dokud to platí, zde se nic nepřiděluje.")
TRANSLATIONS["CPC_Exp_Enabled"] = T(
    "クエスト・探索・キルから経験値を得る", "퀘스트·탐험·처치로 경험치 얻기", "从任务、探索与击杀中获得经验",
    "Получать опыт за задания, исследование и убийства", "Erfahrung aus Quests, Erkundung und Kämpfen",
    "Gagner de l'expérience par les quêtes, l'exploration et les combats",
    "Ganar experiencia con misiones, exploración y muertes",
    "Guadagna esperienza da missioni, esplorazione e uccisioni",
    "Zdobywaj doświadczenie z zadań, eksploracji i zabójstw",
    "Získávat zkušenost z úkolů, průzkumu a zabíjení")
TRANSLATIONS["CPC_Exp_HelpEnabled"] = T(
    "既定はオフです。オフの間は何も与えられず、ゲームのレベル進行は以前とまったく同じです。",
    "기본값은 꺼짐입니다. 꺼져 있는 동안에는 아무것도 주어지지 않으며, 게임의 레벨 진행은 이전과 똑같습니다.",
    "默认关闭。关闭时不给予任何经验，游戏升级方式与之前完全相同。",
    "По умолчанию выключено. Пока выключено, ничего не начисляется, и игра растит уровни ровно как раньше.",
    "Standardmäßig aus. Solange es aus ist, wird nichts vergeben und das Spiel steigt genau wie zuvor auf.",
    "Désactivé par défaut. Tant que c'est désactivé, rien n'est accordé et le jeu monte en niveau exactement comme avant.",
    "Desactivado por defecto. Mientras lo esté, no se otorga nada y el juego sube de nivel exactamente igual que antes.",
    "Disattivato per impostazione predefinita. Finché è spento non viene concesso nulla e il gioco sale di livello esattamente come prima.",
    "Domyślnie wyłączone. Gdy jest wyłączone, nic nie jest przyznawane, a gra awansuje dokładnie tak jak wcześniej.",
    "Ve výchozím stavu vypnuto. Dokud je vypnuto, nic se nepřiděluje a hra postupuje přesně jako dřív.")
TRANSLATIONS["CPC_Exp_SkillsPay"] = T(
    "スキル上昇も引き続きレベルに寄与する", "기술 상승도 계속 레벨에 기여", "技能提升仍计入你的等级",
    "Рост навыков по-прежнему платит в уровень", "Fertigkeitsanstiege zahlen weiterhin auf deine Stufe ein",
    "Les progressions de compétence comptent toujours pour votre niveau",
    "Las subidas de habilidad siguen aportando a tu nivel",
    "Gli aumenti di abilità contribuiscono ancora al tuo livello",
    "Wzrost umiejętności nadal wlicza się do poziomu",
    "Zvýšení dovedností stále přispívá k úrovni")
TRANSLATIONS["CPC_Exp_HelpSkillsPay"] = T(
    "オン: 下の入手元がスキル使用による分に加算されます (上乗せ)。オフ: スキル上昇はレベルに何も寄与せず、下の入手元だけがレベルを上げる手段になります (置き換え)。",
    "켬: 아래의 출처가 기술 사용으로 얻는 몫에 더해집니다(보완). 끔: 기술 상승은 레벨에 아무것도 기여하지 않고, 아래 출처만이 레벨을 올리는 수단이 됩니다(대체).",
    "开启：下面的来源会叠加在技能使用已经给予的经验之上（补充）。关闭：技能提升不再为等级贡献任何经验，下面的来源成为唯一的升级途径（替代）。",
    "Включено: источники ниже добавляются к тому, что уже платит использование навыков (дополнение). Выключено: рост навыков не платит в уровень, и источники ниже - единственный способ его повысить (замена).",
    "An: die Quellen unten kommen zu dem hinzu, was die Fertigkeitsnutzung bereits zahlt (Ergänzung). Aus: Fertigkeitsanstiege zahlen nichts auf deine Stufe ein, und die Quellen unten sind der einzige Weg aufzusteigen (Ersatz).",
    "Activé : les sources ci-dessous s'ajoutent à ce que l'usage des compétences rapporte déjà (complément). Désactivé : les progressions de compétence ne rapportent rien à votre niveau et les sources ci-dessous sont le seul moyen de monter (remplacement).",
    "Activado: las fuentes de abajo se suman a lo que ya aporta el uso de habilidades (suplemento). Desactivado: las subidas de habilidad no aportan nada a tu nivel y las fuentes de abajo son la única forma de subir (sustitución).",
    "Attivo: le fonti sotto si aggiungono a quanto l'uso delle abilità già paga (supplemento). Disattivo: gli aumenti di abilità non pagano nulla verso il tuo livello e le fonti sotto sono l'unico modo per salire (sostituzione).",
    "Włączone: źródła poniżej dodają się do tego, co już daje używanie umiejętności (uzupełnienie). Wyłączone: wzrost umiejętności nic nie wnosi do poziomu, a źródła poniżej są jedyną drogą awansu (zastąpienie).",
    "Zapnuto: zdroje níže se přičítají k tomu, co už platí používání dovedností (doplněk). Vypnuto: zvýšení dovedností k úrovni nic nepřispívá a zdroje níže jsou jediná cesta vzhůru (náhrada).")
TRANSLATIONS["CPC_Exp_RestartNeeded"] = T(
    "スキル上昇の寄与を止めるにはゲームを再起動してください: レベル収入パッチは起動時に取り付けられ、このセッション開始時には必要とされていませんでした。",
    "기술 상승의 기여를 멈추려면 게임을 다시 시작하세요: 레벨 수입 패치는 시작 시에 붙으며, 이번 세션이 시작될 때는 필요하지 않았습니다.",
    "要让技能提升停止贡献经验，请重启游戏：等级收入补丁在启动时挂接，而本次会话开始时并不需要它。",
    "Чтобы рост навыков перестал платить, перезапустите игру: патч дохода в уровень подключается при старте, а на старте этой сессии он не был нужен.",
    "Starte das Spiel neu, damit Fertigkeitsanstiege aufhören zu zahlen: der Stufeneinkommens-Patch hängt sich beim Start ein und wurde beim Start dieser Sitzung nicht gebraucht.",
    "Redémarrez le jeu pour que les progressions de compétence cessent de rapporter : le correctif de revenu de niveau s'attache au démarrage et n'était pas nécessaire au lancement de cette session.",
    "Reinicia el juego para que las subidas de habilidad dejen de aportar: el parche de ingreso de nivel se engancha al arrancar y no hacía falta cuando empezó esta sesión.",
    "Riavvia il gioco perché gli aumenti di abilità smettano di pagare: la patch del reddito di livello si aggancia all'avvio e non serviva all'inizio di questa sessione.",
    "Uruchom grę ponownie, aby wzrost umiejętności przestał się wliczać: łatka dochodu poziomu podpina się przy starcie, a na początku tej sesji nie była potrzebna.",
    "Restartuj hru, aby zvýšení dovedností přestalo platit: záplata příjmu do úrovně se připojuje při startu a na začátku této relace nebyla potřeba.")
TRANSLATIONS["CPC_Exp_SecQuests"] = T(
    "完了したクエスト (種類別)", "완료한 퀘스트 (종류별)", "已完成的任务（按类别）",
    "Выполненные задания, по видам", "Abgeschlossene Quests, nach Art",
    "Quêtes terminées, par type", "Misiones completadas, por tipo",
    "Missioni completate, per tipo", "Ukończone zadania, według rodzaju", "Dokončené úkoly, podle druhu")
TRANSLATIONS["CPC_Exp_QuestMain"] = T(
    "メインクエスト", "메인 퀘스트", "主线任务", "Основное задание", "Hauptquest",
    "Quête principale", "Misión principal", "Missione principale", "Zadanie główne", "Hlavní úkol")
TRANSLATIONS["CPC_Exp_QuestFaction"] = T(
    "勢力クエスト", "세력 퀘스트", "阵营任务", "Задание фракции", "Fraktionsquest",
    "Quête de faction", "Misión de facción", "Missione di fazione", "Zadanie frakcyjne", "Úkol frakce")
TRANSLATIONS["CPC_Exp_HelpFaction"] = T(
    "各ギルド、戦友団、闇の一党、内戦、そして Dawnguard と Dragonborn の系統です。",
    "각 길드, 전우들, 다크 브라더후드, 내전, 그리고 Dawnguard와 Dragonborn 계열입니다.",
    "各大公会、战友团、黑暗兄弟会、内战，以及黎明守卫与龙裔的主线。",
    "Гильдии, Соратники, Тёмное Братство, гражданская война, а также линии Dawnguard и Dragonborn.",
    "Die Gilden, die Gefährten, die Dunkle Bruderschaft, der Bürgerkrieg sowie die Handlungsstränge von Dawnguard und Dragonborn.",
    "Les guildes, les Compagnons, la Confrérie noire, la guerre civile, ainsi que les lignes de Dawnguard et Dragonborn.",
    "Los gremios, los Compañeros, la Hermandad Oscura, la guerra civil y las líneas de Dawnguard y Dragonborn.",
    "Le gilde, i Compagni, la Confraternita Oscura, la guerra civile e le linee di Dawnguard e Dragonborn.",
    "Gildie, Towarzysze, Mroczne Bractwo, wojna domowa oraz wątki Dawnguard i Dragonborn.",
    "Cechy, Družiníci, Temné bratrstvo, občanská válka a linie Dawnguard a Dragonborn.")
TRANSLATIONS["CPC_Exp_QuestDaedric"] = T(
    "デイドラクエスト", "데이드라 퀘스트", "魔神任务", "Даэдрическое задание", "Daedrische Quest",
    "Quête daedrique", "Misión daédrica", "Missione daedrica", "Zadanie daedryczne", "Daedrický úkol")
TRANSLATIONS["CPC_Exp_QuestSide"] = T(
    "サブクエスト", "사이드 퀘스트", "支线任务", "Побочное задание", "Nebenquest",
    "Quête secondaire", "Misión secundaria", "Missione secondaria", "Zadanie poboczne", "Vedlejší úkol")
TRANSLATIONS["CPC_Exp_QuestMisc"] = T(
    "その他の目標", "기타 목표", "杂项目标", "Разное задание", "Sonstiges Ziel",
    "Objectif divers", "Objetivo misceláneo", "Obiettivo vario", "Cel różny", "Vedlejší cíl")
TRANSLATIONS["CPC_Exp_QuestOther"] = T(
    "その他のクエスト", "기타 퀘스트", "其他任务", "Прочее задание", "Andere Quest",
    "Autre quête", "Otra misión", "Altra missione", "Inne zadanie", "Jiný úkol")
TRANSLATIONS["CPC_Exp_HelpOther"] = T(
    "ゲームが種類を持たせていないクエストです - MOD 追加のものが多く該当します。",
    "게임이 종류를 부여하지 않은 퀘스트입니다 - 모드로 추가된 것이 많습니다.",
    "游戏未赋予类别的任务 - 许多模组新增的任务属于此类。",
    "Задания, которым игра не даёт вида - многие добавленные модами.",
    "Quests, denen das Spiel keine Art gibt - viele von Mods hinzugefügte.",
    "Les quêtes auxquelles le jeu ne donne aucun type - beaucoup viennent de mods.",
    "Misiones a las que el juego no da tipo: muchas añadidas por mods.",
    "Missioni a cui il gioco non dà un tipo - molte aggiunte da mod.",
    "Zadania, którym gra nie nadaje rodzaju - wiele dodanych przez mody.",
    "Úkoly, kterým hra nedává druh - mnoho přidaných mody.")
TRANSLATIONS["CPC_Exp_SecExploration"] = T(
    "探索", "탐험", "探索", "Исследование", "Erkundung", "Exploration", "Exploración",
    "Esplorazione", "Eksploracja", "Průzkum")
TRANSLATIONS["CPC_Exp_Location"] = T(
    "場所の発見", "장소 발견", "发现地点", "Место открыто", "Ort entdeckt",
    "Lieu découvert", "Lugar descubierto", "Luogo scoperto", "Odkryte miejsce", "Objevené místo")
TRANSLATIONS["CPC_Exp_Cleared"] = T(
    "場所の制圧", "장소 소탕", "清空地点", "Место зачищено", "Ort geräumt",
    "Lieu nettoyé", "Lugar despejado", "Luogo ripulito", "Oczyszczone miejsce", "Vyčištěné místo")
TRANSLATIONS["CPC_Exp_SecKills"] = T(
    "キル", "처치", "击杀", "Убийства", "Tötungen", "Victimes", "Muertes", "Uccisioni", "Zabójstwa", "Zabití")
TRANSLATIONS["CPC_Exp_KillBase"] = T(
    "キルごと", "처치당", "每次击杀", "За убийство", "Pro Tötung",
    "Par victime", "Por muerte", "Per uccisione", "Za zabójstwo", "Za zabití")
TRANSLATIONS["CPC_Exp_KillPerLevel"] = T(
    "さらに、相手のレベルごと", "추가로, 대상의 레벨당", "另加，按目标的每级",
    "Плюс, за уровень жертвы", "Zusätzlich, pro Stufe des Opfers",
    "Plus, par niveau de la victime", "Además, por nivel de la víctima",
    "In più, per livello della vittima", "Dodatkowo, za poziom ofiary", "Navíc, za úroveň oběti")
TRANSLATIONS["CPC_Exp_FollowerKills"] = T(
    "仲間や召喚体によるキルも数える", "동료와 소환수의 처치도 계산", "统计随从与召唤物的击杀",
    "Считать убийства спутников и призванных", "Tötungen von Begleitern und Beschwörungen mitzählen",
    "Compter les victimes des compagnons et des invocations",
    "Contar las muertes de seguidores e invocaciones",
    "Conta le uccisioni di seguaci ed evocazioni",
    "Licz zabójstwa towarzyszy i przywołańców", "Počítat zabití společníků a přivolaných")
TRANSLATIONS["CPC_Exp_SecBooks"] = T(
    "書物", "책", "书籍", "Книги", "Bücher", "Livres", "Libros", "Libri", "Księgi", "Knihy")
TRANSLATIONS["CPC_Exp_Book"] = T(
    "本を読む", "책 읽기", "阅读书籍", "Прочитанная книга", "Buch gelesen",
    "Livre lu", "Libro leído", "Libro letto", "Przeczytana księga", "Přečtená kniha")
TRANSLATIONS["CPC_Exp_SkillBook"] = T(
    "スキル書を読む", "기술서 읽기", "阅读技能书", "Прочитанная книга навыка", "Fertigkeitsbuch gelesen",
    "Livre de compétence lu", "Libro de habilidad leído", "Libro di abilità letto",
    "Przeczytana księga umiejętności", "Přečtená kniha dovednosti")
TRANSLATIONS["CPC_Exp_LevelLine"] = T(
    "レベル %u: 次のレベルまで %.0f / %.0f 経験値",
    "레벨 %u: 다음 레벨까지 경험치 %.0f / %.0f",
    "等级 %u：距下一级 %.0f / %.0f 经验",
    "Уровень %u: %.0f из %.0f опыта до следующего",
    "Stufe %u: %.0f von %.0f Erfahrung bis zur nächsten",
    "Niveau %u : %.0f sur %.0f d'expérience vers le suivant",
    "Nivel %u: %.0f de %.0f de experiencia hacia el siguiente",
    "Livello %u: %.0f di %.0f di esperienza verso il successivo",
    "Poziom %u: %.0f z %.0f doświadczenia do następnego",
    "Úroveň %u: %.0f z %.0f zkušeností do další")
TRANSLATIONS["CPC_Exp_CountsLine"] = T(
    "このキャラクターの集計開始以降: クエスト %u 件 (%.0f)、場所 %u か所 (%.0f)、制圧 %u か所 (%.0f)、キル %u 体 (%.0f)、書物 %u 冊 (%.0f)",
    "이 캐릭터의 집계 시작 이후: 퀘스트 %u 개 (%.0f), 장소 %u 곳 (%.0f), 소탕 %u 곳 (%.0f), 처치 %u 회 (%.0f), 책 %u 권 (%.0f)",
    "自本角色开始统计以来：任务 %u 个 (%.0f)，地点 %u 处 (%.0f)，清空 %u 处 (%.0f)，击杀 %u 次 (%.0f)，书籍 %u 本 (%.0f)",
    "У этого персонажа с начала подсчёта: заданий %u (%.0f), мест %u (%.0f), зачищено %u (%.0f), убийств %u (%.0f), книг %u (%.0f)",
    "Bei diesem Charakter seit Zählbeginn: %u Quests (%.0f), %u Orte (%.0f), %u geräumt (%.0f), %u Tötungen (%.0f), %u Bücher (%.0f)",
    "Pour ce personnage, depuis le début du comptage : %u quêtes (%.0f), %u lieux (%.0f), %u nettoyés (%.0f), %u victimes (%.0f), %u livres (%.0f)",
    "En este personaje, desde que empezó la cuenta: %u misiones (%.0f), %u lugares (%.0f), %u despejados (%.0f), %u muertes (%.0f), %u libros (%.0f)",
    "Per questo personaggio, dall'inizio del conteggio: %u missioni (%.0f), %u luoghi (%.0f), %u ripuliti (%.0f), %u uccisioni (%.0f), %u libri (%.0f)",
    "U tej postaci od początku liczenia: zadań %u (%.0f), miejsc %u (%.0f), oczyszczonych %u (%.0f), zabójstw %u (%.0f), ksiąg %u (%.0f)",
    "U této postavy od začátku počítání: úkolů %u (%.0f), míst %u (%.0f), vyčištěno %u (%.0f), zabití %u (%.0f), knih %u (%.0f)")
TRANSLATIONS["CPC_Exp_Last"] = T(
    "直近: %s", "최근: %s", "最近：%s", "Последнее: %s", "Zuletzt: %s",
    "Dernier : %s", "Último: %s", "Ultimo: %s", "Ostatnio: %s", "Naposledy: %s")
TRANSLATIONS["CPC_Exp_NotGranting"] = T(
    "何も付与していません - 経験値はバニラどおりスキル使用から得られます。",
    "아무것도 주지 않습니다 - 경험치는 바닐라와 같이 기술 사용에서 옵니다.",
    "未给予任何经验 - 经验与原版一样来自技能使用。",
    "Ничего не начисляется - опыт идёт от использования навыков, как в оригинале.",
    "Es wird nichts vergeben - Erfahrung kommt wie im Original aus der Fertigkeitsnutzung.",
    "Rien n'est accordé - l'expérience vient de l'usage des compétences, comme dans le jeu de base.",
    "No se otorga nada: la experiencia viene del uso de habilidades, como en el juego base.",
    "Non viene concesso nulla - l'esperienza arriva dall'uso delle abilità, come nel gioco base.",
    "Nic nie jest przyznawane - doświadczenie pochodzi z używania umiejętności, jak w podstawowej grze.",
    "Nic se nepřiděluje - zkušenost pochází z používání dovedností jako v základní hře.")

# --- Static Levelling ----------------------------------------------------------------------------
TRANSLATIONS["CPC_Stat_Intro"] = T(
    "スキルを 1 回使うごとに固定量の経験値を与えます。バニラのようにスケールしないので、レベル 5 でもレベル 50 でも同じ速さでスキルが伸びます。",
    "기술을 한 번 사용할 때마다 고정된 양의 경험치를 줍니다. 바닐라처럼 비례하지 않으므로 레벨 5에서도 레벨 50에서도 같은 속도로 기술이 오릅니다.",
    "每次使用技能给予固定数量的经验，而不是原版那样按比例缩放 - 因此技能在 5 级和 50 级时以相同的速度成长。",
    "Фиксированное количество опыта за одно использование навыка вместо оригинального масштабирования - так навык растёт с одинаковой скоростью на 5 и на 50 уровне.",
    "Ein fester Erfahrungsbetrag pro Anwendung einer Fertigkeit statt der Skalierung des Originals - so steigt eine Fertigkeit auf Stufe 5 genauso schnell wie auf Stufe 50.",
    "Un montant fixe d'expérience par utilisation d'une compétence, au lieu de l'échelle du jeu de base - une compétence progresse donc au même rythme au niveau 5 et au niveau 50.",
    "Una cantidad fija de experiencia por cada uso de una habilidad, en lugar del escalado del juego base: así una habilidad avanza al mismo ritmo en el nivel 5 y en el 50.",
    "Una quantità fissa di esperienza per ogni uso di un'abilità, invece della scalatura del gioco base - così un'abilità avanza allo stesso ritmo al livello 5 e al 50.",
    "Stała ilość doświadczenia za każde użycie umiejętności zamiast skalowania z podstawowej gry - dzięki temu umiejętność rośnie w tym samym tempie na 5. i na 50. poziomie.",
    "Pevné množství zkušeností za jedno použití dovednosti místo škálování základní hry - dovednost tak postupuje stejnou rychlostí na úrovni 5 i 50.")
TRANSLATIONS["CPC_Stat_Inert"] = T(
    "スキル経験値は引き続きバニラどおりスケールします",
    "기술 경험치는 계속 바닐라 방식으로 비례합니다",
    "技能经验仍按原版方式缩放",
    "опыт навыков по-прежнему масштабируется как в оригинале",
    "die Fertigkeitserfahrung skaliert weiterhin auf die Art des Originals",
    "l'expérience de compétence continue d'être mise à l'échelle comme dans le jeu de base",
    "la experiencia de habilidad sigue escalando como en el juego base",
    "l'esperienza delle abilità continua a scalare come nel gioco base",
    "doświadczenie umiejętności wciąż skaluje się jak w podstawowej grze",
    "zkušenost dovedností se stále škáluje jako v základní hře")
TRANSLATIONS["CPC_Stat_Enabled"] = T(
    "固定量のスキル成長を使う", "정적 기술 성장 사용", "使用固定式技能升级",
    "Использовать статичный рост навыков", "Statischen Fertigkeitsaufstieg verwenden",
    "Utiliser la progression de compétence statique", "Usar la subida de habilidad estática",
    "Usa la crescita statica delle abilità", "Używaj statycznego rozwoju umiejętności",
    "Používat statické zvyšování dovedností")
TRANSLATIONS["CPC_Stat_HelpEnabled"] = T(
    "既定はオフです。オフの間、スキル経験値について何も主張しません。",
    "기본값은 꺼짐입니다. 꺼져 있는 동안 기술 경험치에 대해 아무것도 주장하지 않습니다.",
    "默认关闭。关闭时对技能经验不做任何干预。",
    "По умолчанию выключено. Пока выключено, об опыте навыков ничего не утверждается.",
    "Standardmäßig aus. Solange es aus ist, wird nichts über die Fertigkeitserfahrung behauptet.",
    "Désactivé par défaut. Tant que c'est désactivé, rien n'est imposé sur l'expérience de compétence.",
    "Desactivado por defecto. Mientras lo esté, no se impone nada sobre la experiencia de habilidad.",
    "Disattivato per impostazione predefinita. Finché è spento, non viene imposto nulla sull'esperienza delle abilità.",
    "Domyślnie wyłączone. Gdy jest wyłączone, nic nie jest narzucane doświadczeniu umiejętności.",
    "Ve výchozím stavu vypnuto. Dokud je vypnuto, o zkušenostech dovedností se nic netvrdí.")
TRANSLATIONS["CPC_Stat_SecPerUse"] = T(
    "1 回の使用あたりの経験値 (スキルレベルに対する割合)",
    "사용 1회당 경험치 (기술 레벨에 대한 비율)",
    "每次使用的经验，按技能等级的百分比计",
    "Опыт за одно использование, в процентах от уровня навыка",
    "Erfahrung pro Anwendung, als Prozentsatz einer Fertigkeitsstufe",
    "Expérience par utilisation, en pourcentage d'un niveau de compétence",
    "Experiencia por uso, como porcentaje de un nivel de habilidad",
    "Esperienza per uso, come percentuale di un livello di abilità",
    "Doświadczenie za użycie, jako procent poziomu umiejętności",
    "Zkušenost za použití, jako procento úrovně dovednosti")
TRANSLATIONS["CPC_Stat_HelpPerUse"] = T(
    "1.0 = 1 回の使用がレベルの 1 パーセントにあたり、スキルが 5 でも 50 でも 100 回の使用で 1 レベル上がります。これがオンの間、スキル使用へのパーク補正は適用されません - 量は固定のままです。",
    "1.0 = 한 번의 사용이 레벨의 1퍼센트에 해당하여, 기술이 5든 50이든 100회 사용하면 한 레벨이 오릅니다. 켜져 있는 동안 기술 사용에 대한 특전 보너스는 적용되지 않습니다 - 양은 고정된 채로 유지됩니다.",
    "1.0 = 一次使用相当于该等级的百分之一，因此无论技能处于 5 还是 50，100 次使用都会提升一级。开启期间不会应用技能使用的特技加成 - 数量保持固定。",
    "1.0 = одно использование равно одному проценту уровня, так что 100 использований поднимают навык на один уровень и на 5, и на 50. Пока это включено, бонусы способностей к использованию навыка не применяются - величина остаётся фиксированной.",
    "1,0 = eine Anwendung ist ein Prozent der Stufe, also heben 100 Anwendungen die Fertigkeit um eine Stufe, ob sie bei 5 oder bei 50 steht. Solange dies an ist, werden Vorteilsboni auf die Fertigkeitsnutzung nicht angewendet - der Betrag bleibt fest.",
    "1,0 = une utilisation vaut un pour cent du niveau, donc 100 utilisations font gagner un niveau, que la compétence soit à 5 ou à 50. Tant que c'est activé, les bonus d'aptitude sur l'usage des compétences ne s'appliquent pas - le montant reste fixe.",
    "1,0 = un uso equivale al uno por ciento del nivel, así que 100 usos suben la habilidad un nivel tanto si está en 5 como en 50. Mientras esto esté activado, las bonificaciones de habilidad especial al uso no se aplican: la cantidad se mantiene fija.",
    "1,0 = un uso è l'uno per cento del livello, quindi 100 usi alzano l'abilità di un livello sia che sia a 5 sia a 50. Finché è attivo, i bonus dei privilegi all'uso delle abilità non vengono applicati - la quantità resta fissa.",
    "1,0 = jedno użycie to jeden procent poziomu, więc 100 użyć podnosi umiejętność o poziom, czy jest na 5, czy na 50. Gdy jest włączone, premie atutów do używania umiejętności nie są stosowane - wartość pozostaje stała.",
    "1,0 = jedno použití je jedno procento úrovně, takže 100 použití zvedne dovednost o úroveň, ať je na 5 nebo na 50. Dokud je toto zapnuté, bonusy vlastností k používání dovedností se nepoužijí - množství zůstává pevné.")
TRANSLATIONS["CPC_Stat_SecPoints"] = T(
    "スキルポイント", "기술 점수", "技能点", "Очки навыков", "Fertigkeitspunkte",
    "Points de compétence", "Puntos de habilidad", "Punti abilità", "Punkty umiejętności", "Body dovedností")
TRANSLATIONS["CPC_Stat_PointsIntro"] = T(
    "スキルはレベルアップメニューで使ったポイントによってのみ上昇します: レベルごとにポイントが与えられ、メニューには全スキルが + と - 付きで並び、能力値を選んだ時点で確定します。スキルポイント対応のレベルアップメニューファイルが必要です (この MOD に同梱、Static Skill Leveling Rewritten のスキンも使えます)。",
    "기술은 레벨업 메뉴에서 사용한 점수로만 오릅니다: 레벨마다 점수가 주어지고, 메뉴에는 모든 기술이 +와 -로 표시되며, 능력치를 선택할 때 확정됩니다. 기술 점수용 레벨업 메뉴 파일이 필요합니다(이 모드에 포함되어 있고, Static Skill Leveling Rewritten의 스킨도 맞습니다).",
    "技能只能通过在升级菜单中花费点数来提升：每级给予点数，菜单会列出所有技能并带有 + 和 -，在你选择属性时确认生效。需要支持技能点的升级菜单文件（本模组自带一份；Static Skill Leveling Rewritten 的皮肤同样适用）。",
    "Навыки растут только за очки, потраченные в меню повышения уровня: каждый уровень даёт очки, меню показывает каждый навык с + и -, а выбор применяется, когда вы выбираете атрибут. Нужен файл меню повышения уровня со скиллпойнтами (мод везёт свой; скины Static Skill Leveling Rewritten тоже подходят).",
    "Fertigkeiten steigen nur durch Punkte, die im Stufenaufstiegsmenü ausgegeben werden: jede Stufe gibt Punkte, das Menü zeigt jede Fertigkeit mit + und -, und die Wahl greift, wenn du dein Attribut wählst. Braucht die Stufenaufstiegsmenü-Datei mit Fertigkeitspunkten (dieser Mod bringt eine mit; die Skins von Static Skill Leveling Rewritten passen ebenfalls).",
    "Les compétences ne progressent que par les points dépensés dans le menu de niveau : chaque niveau accorde des points, le menu affiche chaque compétence avec + et -, et le choix s'applique quand vous choisissez votre attribut. Nécessite le fichier de menu de niveau à points de compétence (ce mod en fournit un ; les habillages de Static Skill Leveling Rewritten conviennent aussi).",
    "Las habilidades solo avanzan con puntos gastados en el menú de subida de nivel: cada nivel otorga puntos, el menú muestra todas las habilidades con + y -, y la elección se aplica cuando eliges tu atributo. Necesita el archivo de menú de subida de nivel con puntos de habilidad (este mod incluye uno; las skins de Static Skill Leveling Rewritten también sirven).",
    "Le abilità avanzano solo con i punti spesi nel menu di passaggio di livello: ogni livello concede punti, il menu mostra ogni abilità con + e -, e la scelta si applica quando scegli l'attributo. Richiede il file del menu di livello con i punti abilità (questa mod ne include uno; anche le skin di Static Skill Leveling Rewritten vanno bene).",
    "Umiejętności rosną wyłącznie dzięki punktom wydanym w menu awansu: każdy poziom przyznaje punkty, menu pokazuje każdą umiejętność z + i -, a wybór zatwierdza się, gdy wybierzesz atrybut. Wymaga pliku menu awansu z punktami umiejętności (ten mod dostarcza własny; skórki Static Skill Leveling Rewritten też pasują).",
    "Dovednosti rostou jen díky bodům utraceným v nabídce postupu: každá úroveň dá body, nabídka ukáže každou dovednost s + a -, a volba se použije, když si vybereš atribut. Vyžaduje soubor nabídky postupu s body dovedností (tento mod jeden přináší; skiny Static Skill Leveling Rewritten sedí také).")
TRANSLATIONS["CPC_Stat_UsePoints"] = T(
    "スキルポイントを使う", "기술 점수 사용", "使用技能点", "Использовать очки навыков",
    "Fertigkeitspunkte verwenden", "Utiliser les points de compétence", "Usar puntos de habilidad",
    "Usa i punti abilità", "Używaj punktów umiejętności", "Používat body dovedností")
TRANSLATIONS["CPC_Stat_HelpUsePoints"] = T(
    "既定はオフです。オンの間、通常のスキル経験値は蓄積されず - 使用はポイントを通してのみ数えられ - ポイントで上げたレベルはキャラクターレベルに何も寄与しません。再起動後に反映されます。",
    "기본값은 꺼짐입니다. 켜져 있는 동안 일반 기술 경험치는 쌓이지 않고 - 사용은 점수를 통해서만 계산되며 - 점수로 올린 레벨은 캐릭터 레벨에 아무것도 기여하지 않습니다. 재시작 후에 적용됩니다.",
    "默认关闭。开启时普通的技能经验不再累积 - 使用只通过点数计入 - 且用点数提升的等级不会为角色等级贡献任何经验。重启后生效。",
    "По умолчанию выключено. Пока включено, обычный опыт навыков не копится - использование считается только через очки, - а уровень, поднятый очками, ничего не платит в уровень персонажа. Вступает в силу после перезапуска.",
    "Standardmäßig aus. Solange es an ist, wird gewöhnliche Fertigkeitserfahrung nicht angesammelt - Nutzung zählt nur über Punkte - und eine per Punkt gehobene Stufe zahlt nichts auf die Charakterstufe ein. Wirkt nach einem Neustart.",
    "Désactivé par défaut. Tant que c'est activé, l'expérience de compétence ordinaire n'est pas engrangée - l'usage ne compte qu'à travers les points - et un niveau gagné par points ne rapporte rien au niveau de personnage. Prend effet après un redémarrage.",
    "Desactivado por defecto. Mientras esté activado, la experiencia de habilidad ordinaria no se acumula (el uso solo cuenta a través de puntos) y un nivel subido con puntos no aporta nada al nivel de personaje. Surte efecto tras reiniciar.",
    "Disattivato per impostazione predefinita. Finché è attivo, l'esperienza ordinaria delle abilità non viene accumulata - l'uso conta solo attraverso i punti - e un livello alzato con i punti non paga nulla verso il livello del personaggio. Ha effetto dopo un riavvio.",
    "Domyślnie wyłączone. Gdy jest włączone, zwykłe doświadczenie umiejętności nie jest gromadzone - używanie liczy się tylko przez punkty - a poziom podniesiony punktami nic nie wnosi do poziomu postaci. Działa po restarcie.",
    "Ve výchozím stavu vypnuto. Dokud je zapnuto, běžná zkušenost dovedností se nehromadí - používání se počítá jen přes body - a úroveň zvednutá body nepřispívá k úrovni postavy ničím. Projeví se po restartu.")
TRANSLATIONS["CPC_Stat_PointsPerLevel"] = T(
    "レベルごとのポイント", "레벨당 점수", "每级点数", "Очков за уровень", "Punkte pro Stufe",
    "Points par niveau", "Puntos por nivel", "Punti per livello", "Punkty na poziom", "Body za úroveň")
TRANSLATIONS["CPC_Stat_PointsLevelMult"] = T(
    "さらに、キャラクターレベルごと", "추가로, 캐릭터 레벨당", "另加，按角色的每级",
    "Плюс, за уровень персонажа", "Zusätzlich, pro Charakterstufe",
    "Plus, par niveau de personnage", "Además, por nivel de personaje",
    "In più, per livello del personaggio", "Dodatkowo, za poziom postaci", "Navíc, za úroveň postavy")
TRANSLATIONS["CPC_Stat_HelpPointsMult"] = T(
    "あるレベルで与えられるポイント = レベルごとのポイント + これ x レベル (整数)。負の値にすると伸びが緩やかになります。",
    "어떤 레벨에서 주어지는 점수 = 레벨당 점수 + 이 값 x 레벨 (정수). 음수로 두면 곡선이 완만해집니다.",
    "某一级给予的点数 = 每级点数 + 此值 x 等级，取整数。负值会放缓曲线。",
    "Очки на уровне = очки за уровень + это x уровень, целыми числами. Отрицательное значение замедляет кривую.",
    "Punkte auf einer Stufe = Punkte pro Stufe + dies x die Stufe, in ganzen Zahlen. Negativ verlangsamt die Kurve.",
    "Points accordés à un niveau = points par niveau + ceci x le niveau, en nombres entiers. Une valeur négative ralentit la courbe.",
    "Puntos otorgados en un nivel = puntos por nivel + esto x el nivel, en números enteros. Un valor negativo ralentiza la curva.",
    "Punti concessi a un livello = punti per livello + questo x il livello, in numeri interi. Un valore negativo rallenta la curva.",
    "Punkty przyznane na poziomie = punkty na poziom + to x poziom, liczby całkowite. Wartość ujemna spowalnia krzywą.",
    "Body na úrovni = body za úroveň + toto x úroveň, celá čísla. Záporná hodnota křivku zpomaluje.")
TRANSLATIONS["CPC_Stat_BankCap"] = T(
    "貯蓄の上限 (0 = なし)", "저장 한도 (0 = 없음)", "存量上限（0 = 无）",
    "Предел запаса (0 = нет)", "Obergrenze des Vorrats (0 = keine)",
    "Plafond de réserve (0 = aucun)", "Tope acumulado (0 = ninguno)",
    "Limite di riserva (0 = nessuno)", "Limit zapasu (0 = brak)", "Strop zásoby (0 = žádný)")
TRANSLATIONS["CPC_Stat_MaxIncreases"] = T(
    "1 レベルアップあたり、1 スキルの最大上昇回数", "레벨업 1회당 한 기술의 최대 상승 횟수",
    "每次升级中单个技能的最大提升次数", "Больше всего повышений одного навыка за уровень",
    "Höchste Anzahl Anstiege je Fertigkeit pro Stufenaufstieg",
    "Nombre maximal de progressions d'une compétence par niveau",
    "Máximo de subidas por habilidad y por nivel",
    "Massimo di aumenti per abilità a ogni passaggio di livello",
    "Najwięcej wzrostów jednej umiejętności na awans",
    "Nejvíce zvýšení jedné dovednosti za postup")
TRANSLATIONS["CPC_Stat_CostIntro"] = T(
    "スキル 1 レベル分の費用 (そのスキルの現在レベル別):",
    "기술 한 레벨의 비용 (해당 기술의 현재 레벨별):",
    "提升一级技能的花费，按该技能当前等级划分：",
    "Стоимость одного уровня навыка, по его текущему уровню:",
    "Kosten einer Fertigkeitsstufe, nach der aktuellen Stufe der Fertigkeit:",
    "Coût d'un niveau de compétence, selon le niveau actuel de la compétence :",
    "Coste de un nivel de habilidad, según el nivel actual de la habilidad:",
    "Costo di un livello di abilità, in base al livello attuale dell'abilità:",
    "Koszt jednego poziomu umiejętności, według jej obecnego poziomu:",
    "Cena jedné úrovně dovednosti podle její současné úrovně:")
TRANSLATIONS["CPC_Stat_BankLine"] = T(
    "このキャラクター: %d ポイント貯蓄済み。次のレベルでは %d ポイント与えられます。",
    "이 캐릭터: %d 점 보유. 다음 레벨에서는 %d 점을 줍니다.",
    "本角色：已存 %d 点；下一级将给予 %d 点。",
    "Этот персонаж: накоплено %d очк.; следующий уровень даст %d.",
    "Dieser Charakter: %d Punkt(e) angespart; die nächste Stufe gibt %d.",
    "Ce personnage : %d point(s) en réserve ; le prochain niveau en accorde %d.",
    "Este personaje: %d punto(s) acumulados; el siguiente nivel otorga %d.",
    "Questo personaggio: %d punto/i in riserva; il prossimo livello ne concede %d.",
    "Ta postać: %d punkt(ów) w zapasie; następny poziom daje %d.",
    "Tato postava: %d bod(ů) v zásobě; další úroveň dá %d.")

# --- Presets -------------------------------------------------------------------------------------
TRANSLATIONS["CPC_Pre_Intro"] = T(
    "プリセットとは、この MOD の Presets フォルダにあるファイルで、形式は INI と同じです。選択されているものがそのまま MOD の使用中の設定になります。ファイルは共有されますが、選択はこのキャラクターのものなので、2 つのセーブが別々のプリセットを同時に使えます。",
    "프리셋은 이 모드의 Presets 폴더에 있는 파일이며 형식은 INI와 같습니다. 선택된 것이 곧 모드가 사용하는 설정입니다. 파일은 공유되지만 선택은 이 캐릭터의 것이므로, 두 세이브가 서로 다른 프리셋을 동시에 쓸 수 있습니다.",
    "预设就是本模组 Presets 文件夹中的一个文件，格式与其 INI 相同。被选中的那个就是模组正在使用的配置 - 文件是共享的，但选择属于这个角色，因此两个存档可以同时处于不同的预设上。",
    "Пресет - это файл в папке Presets этого мода, в том же формате, что и его INI. Выбранный пресет и есть то, чем мод пользуется; файлы общие, а выбор принадлежит ЭТОМУ персонажу, поэтому два сохранения могут одновременно сидеть на разных пресетах.",
    "Ein Preset ist eine Datei im Presets-Ordner dieses Mods, im selben Format wie seine INI. Welches ausgewählt ist, ist das, was der Mod benutzt - und die Auswahl gehört DIESEM Charakter, während die Dateien geteilt werden, sodass zwei Spielstände gleichzeitig auf verschiedenen Presets sitzen können.",
    "Un préréglage est un fichier du dossier Presets de ce mod, au même format que son INI. Celui qui est sélectionné est ce que le mod utilise - et la sélection appartient à CE personnage tandis que les fichiers sont partagés, si bien que deux sauvegardes peuvent être sur des préréglages différents en même temps.",
    "Un preajuste es un archivo en la carpeta Presets de este mod, con el mismo formato que su INI. El que esté seleccionado es lo que el mod está usando, y la selección pertenece a ESTE personaje mientras los archivos son compartidos, de modo que dos partidas pueden estar en preajustes distintos a la vez.",
    "Un preset è un file nella cartella Presets di questa mod, nello stesso formato del suo INI. Quello selezionato è ciò che la mod sta usando - e la selezione appartiene a QUESTO personaggio mentre i file sono condivisi, così due salvataggi possono stare su preset diversi nello stesso momento.",
    "Zestaw to plik w folderze Presets tego moda, w tym samym formacie co jego INI. Wybrany zestaw jest tym, czego mod używa - a wybór należy do TEJ postaci, podczas gdy pliki są wspólne, więc dwa zapisy mogą jednocześnie korzystać z różnych zestawów.",
    "Předvolba je soubor ve složce Presets tohoto modu, ve stejném formátu jako jeho INI. Vybraná předvolba je to, co mod používá - a výběr patří TÉTO postavě, zatímco soubory jsou sdílené, takže dvě pozice mohou být na různých předvolbách zároveň.")
TRANSLATIONS["CPC_Pre_Using"] = T(
    "このキャラクターが使用中: %s", "이 캐릭터가 사용 중: %s", "本角色正在使用：%s",
    "Этот персонаж использует: %s", "Dieser Charakter benutzt: %s",
    "Ce personnage utilise : %s", "Este personaje está usando: %s",
    "Questo personaggio sta usando: %s", "Ta postać używa: %s", "Tato postava používá: %s")
TRANSLATIONS["CPC_Pre_SecDifficulty"] = T(
    "ゲームの難易度", "게임 난이도", "游戏难度", "Сложность игры", "Spielschwierigkeit",
    "Difficulté du jeu", "Dificultad del juego", "Difficoltà di gioco", "Trudność gry", "Obtížnost hry")
TRANSLATIONS["CPC_Pre_Follow"] = T(
    "ゲームの難易度に追随する", "게임 난이도를 따르기", "跟随游戏难度",
    "Следовать сложности игры", "Der Spielschwierigkeit folgen", "Suivre la difficulté du jeu",
    "Seguir la dificultad del juego", "Segui la difficoltà di gioco",
    "Podążaj za trudnością gry", "Sledovat obtížnost hry")
TRANSLATIONS["CPC_Pre_HelpFollow"] = T(
    "難易度ごとに 1 つの設定です。これがオンの間、ゲーム自身の設定で決めた難易度が 6 つのプリセット - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - のどれを使うかを決め、変更すると切り替わります: 使っていた内容は元の難易度のプリセットに保存され、新しい難易度のものが読み込まれます (初回は現在の設定から作られます)。MOD の INI に保存されるので、セッションをまたいでオンのままになります。",
    "난이도마다 하나의 설정입니다. 켜져 있는 동안 게임 자체 설정에서 정한 난이도가 여섯 프리셋 - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - 중 어느 것을 쓸지 결정하며, 바꾸면 전환됩니다: 쓰던 내용은 이전 난이도의 프리셋에 저장되고 새 난이도의 것이 로드됩니다(처음에는 현재 설정으로 만들어집니다). 모드의 INI에 저장되므로 세션 사이에도 켜진 채 유지됩니다.",
    "每个难度一套配置。开启时，你在游戏自身设置中选择的难度决定使用六个预设中的哪一个 - Difficulty - Novice、Apprentice、Adept、Expert、Master、Legendary - 更改难度即会切换：当前内容会保存进旧难度的预设，并载入新难度的预设（首次会用当前配置创建）。该开关保存在模组的 INI 中，因此会在会话之间保持。",
    "Одна конфигурация на сложность. Пока это включено, сложность, заданная в собственных настройках игры, решает, какой из шести пресетов используется - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - и её смена переключает их: то, что было, сохраняется в пресет старой сложности, а пресет новой загружается (в первый раз он создаётся из текущей конфигурации). Хранится в INI мода, так что остаётся включённым между сессиями.",
    "Eine Konfiguration je Schwierigkeitsgrad. Solange dies an ist, entscheidet der in den Spieleinstellungen gesetzte Grad, welches der sechs Presets benutzt wird - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - und ein Wechsel schaltet um: was du hattest, wird in das Preset des alten Grades gespeichert und das des neuen geladen (beim ersten Mal aus der aktuellen Konfiguration erstellt). Wird in der INI des Mods gespeichert und bleibt so zwischen Sitzungen an.",
    "Une configuration par difficulté. Tant que c'est activé, la difficulté réglée dans les paramètres du jeu décide lequel des six préréglages est utilisé - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - et la changer bascule : ce que vous aviez est enregistré dans le préréglage de l'ancienne difficulté et celui de la nouvelle est chargé (créé à partir de la configuration courante la première fois). Enregistré avec l'INI du mod, donc conservé entre les sessions.",
    "Una configuración por dificultad. Mientras esto esté activado, la dificultad fijada en los ajustes del propio juego decide cuál de los seis preajustes se usa - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - y cambiarla alterna: lo que tenías se guarda en el preajuste de la dificultad anterior y se carga el de la nueva (creado a partir de la configuración actual la primera vez). Se guarda con el INI del mod, así que sigue activado entre sesiones.",
    "Una configurazione per difficoltà. Finché è attivo, la difficoltà impostata nelle opzioni del gioco decide quale dei sei preset è in uso - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - e cambiarla commuta: quello che avevi viene salvato nel preset della vecchia difficoltà e viene caricato quello della nuova (creato dalla configurazione corrente la prima volta). Salvato con l'INI della mod, quindi resta attivo tra una sessione e l'altra.",
    "Jedna konfiguracja na poziom trudności. Gdy jest włączone, trudność ustawiona w opcjach samej gry decyduje, który z sześciu zestawów jest używany - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - a jej zmiana przełącza: to, co miałeś, zostaje zapisane w zestawie starej trudności, a zestaw nowej jest wczytywany (za pierwszym razem tworzony z bieżącej konfiguracji). Zapisywane w INI moda, więc pozostaje włączone między sesjami.",
    "Jedna konfigurace na obtížnost. Dokud je toto zapnuté, obtížnost nastavená v nastavení samotné hry rozhoduje, která ze šesti předvoleb se používá - Difficulty - Novice, Apprentice, Adept, Expert, Master, Legendary - a její změna přepíná: co jsi měl, se uloží do předvolby staré obtížnosti a načte se předvolba nové (poprvé vytvořená z aktuální konfigurace). Ukládá se s INI modu, takže mezi sezeními zůstává zapnuté.")
TRANSLATIONS["CPC_Pre_ArrowPreset"] = T(
    ' -> プリセット "%s"', ' -> 프리셋 "%s"', ' -> 预设 "%s"', ' -> пресет "%s"', ' -> Preset "%s"',
    ' -> préréglage "%s"', ' -> preajuste "%s"', ' -> preset "%s"', ' -> zestaw "%s"', ' -> předvolba "%s"')
TRANSLATIONS["CPC_Pre_GameIsOn"] = T(
    "ゲームの難易度は %s%s", "게임 난이도는 %s%s", "游戏当前难度为 %s%s",
    "Игра идёт на %s%s", "Das Spiel läuft auf %s%s", "Le jeu est en %s%s",
    "El juego está en %s%s", "Il gioco è su %s%s", "Gra jest na %s%s", "Hra běží na %s%s")
TRANSLATIONS["CPC_Pre_SecPresets"] = T(
    "プリセット", "프리셋", "预设", "Пресеты", "Presets", "Préréglages", "Preajustes",
    "Preset", "Zestawy", "Předvolby")
TRANSLATIONS["CPC_Pre_InUse"] = T(
    "使用中", "사용 중", "使用中", "Используется", "In Gebrauch", "Utilisé", "En uso",
    "In uso", "W użyciu", "Používá se")
TRANSLATIONS["CPC_Pre_Use"] = T(
    "使う", "사용", "使用", "Использовать", "Benutzen", "Utiliser", "Usar", "Usa", "Użyj", "Použít")
TRANSLATIONS["CPC_Pre_NowUsing"] = T(
    "%s を使用します。", "%s 을(를) 사용합니다.", "现在使用 %s。", "Теперь используется %s.",
    "Benutzt jetzt %s.", "Utilise maintenant %s.", "Ahora se usa %s.", "Ora si usa %s.",
    "Teraz używany jest %s.", "Nyní se používá %s.")
TRANSLATIONS["CPC_Pre_SelectFailed"] = T(
    "そのプリセットを読み込めませんでした - 組み込みの既定に戻しました。",
    "그 프리셋을 읽을 수 없어 내장 기본값으로 되돌렸습니다.",
    "无法读取该预设 - 已回退到内置默认。",
    "Этот пресет прочитать не удалось - откат на встроенный по умолчанию.",
    "Dieses Preset konnte nicht gelesen werden - zurück auf die eingebaute Standardeinstellung.",
    "Ce préréglage n'a pas pu être lu - retour au préréglage intégré par défaut.",
    "No se pudo leer ese preajuste: se volvió al predeterminado integrado.",
    "Non è stato possibile leggere quel preset - si è tornati a quello predefinito integrato.",
    "Nie udało się odczytać tego zestawu - powrót do wbudowanego domyślnego.",
    "Tuto předvolbu nešlo načíst - návrat na vestavěnou výchozí.")
TRANSLATIONS["CPC_Pre_SecThisConfig"] = T(
    "現在の設定", "현재 설정", "当前配置", "Эта конфигурация", "Diese Konfiguration",
    "Cette configuration", "Esta configuración", "Questa configurazione",
    "Ta konfiguracja", "Tato konfigurace")
TRANSLATIONS["CPC_Pre_SaveInto"] = T(
    "選択中のプリセットに保存", "선택한 프리셋에 저장", "保存到所选预设",
    "Сохранить в выбранный пресет", "In das gewählte Preset speichern",
    "Enregistrer dans le préréglage sélectionné", "Guardar en el preajuste seleccionado",
    "Salva nel preset selezionato", "Zapisz do wybranego zestawu", "Uložit do vybrané předvolby")
TRANSLATIONS["CPC_Pre_SavedInto"] = T(
    "%s に保存しました。", "%s 에 저장했습니다.", "已保存到 %s。", "Сохранено в %s.",
    "In %s gespeichert.", "Enregistré dans %s.", "Guardado en %s.", "Salvato in %s.",
    "Zapisano do %s.", "Uloženo do %s.")
TRANSLATIONS["CPC_Pre_SaveIntoFailed"] = T(
    "組み込みの既定はファイルではありません - 代わりに新しいプリセットとして保存してください。",
    "내장 기본값은 파일이 아닙니다 - 대신 새 프리셋으로 저장하세요.",
    "内置默认并不是一个文件 - 请改用“另存为新预设”。",
    "Встроенный пресет по умолчанию - не файл; сохраните как новый пресет.",
    "Die eingebaute Standardeinstellung ist keine Datei - speichere stattdessen als neues Preset.",
    "Le préréglage intégré par défaut n'est pas un fichier - enregistrez plutôt un nouveau préréglage.",
    "El preajuste integrado por defecto no es un archivo: guarda como preajuste nuevo en su lugar.",
    "Il preset predefinito integrato non è un file - salva invece come nuovo preset.",
    "Wbudowany domyślny zestaw nie jest plikiem - zapisz zamiast tego jako nowy zestaw.",
    "Vestavěná výchozí předvolba není soubor - ulož místo toho jako novou předvolbu.")
TRANSLATIONS["CPC_Pre_HelpSaveInto"] = T(
    "プリセットは読み取り専用のテンプレートではなく、生きた設定です: これは今ページ上にある内容を、使用中のプリセットに書き戻します。",
    "프리셋은 읽기 전용 템플릿이 아니라 살아 있는 설정입니다: 지금 페이지에 있는 내용을 사용 중인 프리셋에 다시 씁니다.",
    "预设是活的配置，而非只读模板：这会把页面上当前的内容写回你正在使用的预设。",
    "Пресет - живая конфигурация, а не шаблон только для чтения: это записывает то, что сейчас на страницах, обратно в используемый пресет.",
    "Ein Preset ist eine lebende Konfiguration, keine schreibgeschützte Vorlage: dies schreibt, was jetzt auf den Seiten steht, in das Preset zurück, das du benutzt.",
    "Un préréglage est une configuration vivante, pas un modèle en lecture seule : ceci réécrit ce qui est actuellement sur les pages dans le préréglage que vous utilisez.",
    "Un preajuste es una configuración viva, no una plantilla de solo lectura: esto escribe lo que hay ahora en las páginas de vuelta al preajuste que estás usando.",
    "Un preset è una configurazione viva, non un modello di sola lettura: questo riscrive ciò che è ora nelle pagine nel preset che stai usando.",
    "Zestaw to żywa konfiguracja, a nie szablon tylko do odczytu: to zapisuje to, co jest teraz na stronach, z powrotem do używanego zestawu.",
    "Předvolba je živá konfigurace, ne šablona jen pro čtení: toto zapíše to, co je teď na stránkách, zpět do předvolby, kterou používáš.")
TRANSLATIONS["CPC_Pre_NewName"] = T(
    "新しいプリセット名", "새 프리셋 이름", "新预设名称", "Имя нового пресета",
    "Name des neuen Presets", "Nom du nouveau préréglage", "Nombre del nuevo preajuste",
    "Nome del nuovo preset", "Nazwa nowego zestawu", "Název nové předvolby")
TRANSLATIONS["CPC_Pre_SaveAsNew"] = T(
    "新しいプリセットとして保存", "새 프리셋으로 저장", "另存为新预设",
    "Сохранить как новый пресет", "Als neues Preset speichern",
    "Enregistrer comme nouveau préréglage", "Guardar como preajuste nuevo",
    "Salva come nuovo preset", "Zapisz jako nowy zestaw", "Uložit jako novou předvolbu")
TRANSLATIONS["CPC_Pre_WroteAndSelected"] = T(
    "%s を書き出して選択しました。", "%s 을(를) 쓰고 선택했습니다.", "已写入并选择 %s。",
    "Записан и выбран %s.", "%s geschrieben und ausgewählt.", "%s écrit et sélectionné.",
    "Se escribió y seleccionó %s.", "Scritto e selezionato %s.",
    "Zapisano i wybrano %s.", "Zapsáno a vybráno %s.")
TRANSLATIONS["CPC_Pre_BadName"] = T(
    "その名前はファイル名として使えません。", "그 이름은 파일 이름으로 쓸 수 없습니다.",
    "该名称不能用作文件名。", "Это имя нельзя использовать как имя файла.",
    "Dieser Name kann nicht als Dateiname verwendet werden.",
    "Ce nom ne peut pas servir de nom de fichier.", "Ese nombre no se puede usar como nombre de archivo.",
    "Quel nome non può essere usato come nome di file.",
    "Tej nazwy nie można użyć jako nazwy pliku.", "Toto jméno nelze použít jako název souboru.")
TRANSLATIONS["CPC_Pre_HelpSaveAsNew"] = T(
    "現在の設定を独立したファイルとして書き出します。プリセットを共有する方法でもあります。",
    "현재 설정을 독립된 파일로 씁니다. 프리셋을 공유하는 방법이기도 합니다.",
    "把当前配置写成独立文件，这也是分享配置的方式。",
    "Записывает текущую конфигурацию отдельным файлом - так же ею и делятся.",
    "Schreibt die aktuelle Konfiguration als eigene Datei - so gibt man sie auch weiter.",
    "Écrit la configuration actuelle dans son propre fichier, ce qui est aussi la façon de la partager.",
    "Escribe la configuración actual como su propio archivo, que es también la forma de compartirla.",
    "Scrive la configurazione attuale come file a sé, che è anche il modo di condividerla.",
    "Zapisuje bieżącą konfigurację jako własny plik, co jest też sposobem na jej udostępnienie.",
    "Zapíše aktuální konfiguraci jako vlastní soubor, což je zároveň způsob, jak ji sdílet.")
TRANSLATIONS["CPC_Pre_Delete"] = T(
    "選択中のプリセットを削除", "선택한 프리셋 삭제", "删除所选预设",
    "Удалить выбранный пресет", "Das gewählte Preset löschen",
    "Supprimer le préréglage sélectionné", "Eliminar el preajuste seleccionado",
    "Elimina il preset selezionato", "Usuń wybrany zestaw", "Smazat vybranou předvolbu")
TRANSLATIONS["CPC_Pre_Deleted"] = T(
    "%s を削除しました。組み込みの既定に戻ります。",
    "%s 을(를) 삭제했습니다. 내장 기본값으로 돌아갑니다.",
    "已删除 %s；已回到内置默认。",
    "Удалён %s; снова на встроенном пресете по умолчанию.",
    "%s gelöscht; zurück auf der eingebauten Standardeinstellung.",
    "%s supprimé ; retour au préréglage intégré par défaut.",
    "Se eliminó %s; de vuelta al predeterminado integrado.",
    "Eliminato %s; di nuovo sul predefinito integrato.",
    "Usunięto %s; powrót do wbudowanego domyślnego.",
    "Smazáno %s; zpět na vestavěné výchozí.")
TRANSLATIONS["CPC_Pre_DeleteFailed"] = T(
    "そのプリセットを削除できませんでした。", "그 프리셋을 삭제할 수 없었습니다.", "无法删除该预设。",
    "Не удалось удалить этот пресет.", "Dieses Preset konnte nicht gelöscht werden.",
    "Impossible de supprimer ce préréglage.", "No se pudo eliminar ese preajuste.",
    "Impossibile eliminare quel preset.", "Nie udało się usunąć tego zestawu.",
    "Tuto předvolbu se nepodařilo smazat.")
TRANSLATIONS["CPC_Pre_HelpDelete"] = T(
    "ファイルを削除します。組み込みの既定は決して削除できないので、常に戻れる先があります。",
    "파일을 삭제합니다. 내장 기본값은 결코 삭제할 수 없으므로 언제나 돌아갈 곳이 있습니다.",
    "删除该文件。内置默认永远无法删除，因此总有可以回退的地方。",
    "Удаляет файл. Встроенный пресет по умолчанию удалить нельзя, поэтому всегда есть куда вернуться.",
    "Löscht die Datei. Die eingebaute Standardeinstellung kann nie gelöscht werden, es gibt also immer einen Rückweg.",
    "Supprime le fichier. Le préréglage intégré par défaut ne peut jamais être supprimé, il y a donc toujours un repli.",
    "Elimina el archivo. El preajuste integrado por defecto nunca se puede borrar, así que siempre hay adónde volver.",
    "Elimina il file. Il preset predefinito integrato non può mai essere eliminato, quindi c'è sempre dove tornare.",
    "Usuwa plik. Wbudowanego domyślnego zestawu nigdy nie da się usunąć, więc zawsze jest dokąd wrócić.",
    "Smaže soubor. Vestavěnou výchozí předvolbu nelze nikdy smazat, takže je vždy kam se vrátit.")

# --- Enchanting ----------------------------------------------------------------------------------
TRANSLATIONS["CPC_Ench_Intro"] = T(
    "付呪されたアイテムを使う際の消費が、付呪スキルに応じてどう変化するかです。付呪の上限を外したときに破綻するのがこの式であり、スキル上限の隣にある理由でもあります。",
    "마법부여된 아이템을 사용할 때의 소모가 마법부여 기술에 따라 어떻게 변하는지입니다. 마법부여의 상한을 풀면 무너지는 것이 바로 이 식이며, 기술 상한 옆에 있는 이유이기도 합니다.",
    "使用附魔物品的消耗如何随你的附魔技能变化。当附魔解除上限时，正是这条公式会崩坏 - 这也是它紧邻技能上限的原因。",
    "Как стоимость использования зачарованного предмета зависит от навыка Зачарования. Именно это уравнение ломается, когда Зачарование снимают с потолка, - потому оно и стоит рядом с потолками навыков.",
    "Wie die Kosten für die Benutzung eines verzauberten Gegenstands mit deiner Verzauberungs-Fertigkeit skalieren. Genau diese Gleichung bricht, wenn Verzauberung entgrenzt wird - deshalb steht sie neben den Fertigkeitsgrenzen.",
    "Comment le coût d'utilisation d'un objet enchanté évolue avec votre compétence d'Enchantement. C'est cette équation qui casse quand l'Enchantement est déplafonné, d'où sa place à côté des plafonds de compétence.",
    "Cómo escala el coste de usar un objeto encantado con tu habilidad de Encantamiento. Es esta ecuación la que se rompe cuando Encantamiento pierde su límite, y por eso está junto a los límites de habilidad.",
    "Come il costo d'uso di un oggetto ammaliato scala con la tua abilità di Ammaliamento. È questa equazione a rompersi quando l'Ammaliamento viene slimitato, ed è per questo che sta accanto ai limiti di abilità.",
    "Jak koszt użycia zaklętego przedmiotu skaluje się z twoją umiejętnością Zaklinania. To właśnie to równanie się psuje, gdy Zaklinanie zostaje odblokowane - dlatego stoi obok limitów umiejętności.",
    "Jak se cena používání očarovaného předmětu škáluje s tvou dovedností Očarování. Právě tato rovnice se láme, když se Očarování odstropuje - proto stojí vedle stropů dovedností.")
TRANSLATIONS["CPC_Ench_NotCaptured"] = T(
    "このランタイムでは付呪の消費設定をひとつも読み取れなかったため、このタブは何も書き込みません。ログを確認してください。",
    "이 런타임에서는 마법부여 소모 설정을 하나도 읽을 수 없어 이 탭은 아무것도 기록하지 않습니다. 로그를 확인하세요.",
    "在此运行时下无法读取任何附魔消耗设置，因此该标签页不会写入任何内容。请查看日志。",
    "На этой версии игры ни одна из настроек стоимости зачарования не прочиталась, поэтому вкладка ничего не пишет. Смотрите журнал.",
    "Keine der Verzauberungskosten-Einstellungen konnte auf dieser Laufzeit gelesen werden, deshalb schreibt diese Registerkarte nichts. Siehe Protokoll.",
    "Aucun des paramètres de coût d'enchantement n'a pu être lu sur cette version du jeu ; cet onglet n'écrira donc rien. Voir le journal.",
    "No se pudo leer ninguno de los ajustes de coste de encantamiento en esta versión del juego, así que esta pestaña no escribirá nada. Consulta el registro.",
    "Nessuna delle impostazioni di costo dell'ammaliamento è stata letta su questa runtime, quindi questa scheda non scriverà nulla. Vedi il log.",
    "Na tej wersji gry nie udało się odczytać żadnego z ustawień kosztu zaklinania, więc ta karta niczego nie zapisze. Zobacz dziennik.",
    "Na této verzi hry se nepodařilo přečíst žádné nastavení ceny očarování, takže tato karta nic nezapíše. Viz protokol.")
TRANSLATIONS["CPC_Ench_Partial"] = T(
    "このランタイムに存在するのはこれらの設定の一部だけです。残りはそのままにします。",
    "이 런타임에는 이 설정 중 일부만 존재합니다. 나머지는 그대로 둡니다.",
    "此运行时下只有其中一部分设置存在，其余保持不变。",
    "На этой версии игры существует лишь часть этих настроек; остальные не трогаются.",
    "Nur ein Teil dieser Einstellungen existiert auf dieser Laufzeit; der Rest bleibt unangetastet.",
    "Seuls certains de ces paramètres existent sur cette version du jeu ; les autres sont laissés tels quels.",
    "Solo algunos de estos ajustes existen en esta versión del juego; el resto se deja en paz.",
    "Solo alcune di queste impostazioni esistono su questa runtime; le altre vengono lasciate stare.",
    "Tylko część tych ustawień istnieje na tej wersji gry; reszta zostaje nietknięta.",
    "Na této verzi hry existuje jen část těchto nastavení; zbytek zůstává nedotčen.")
TRANSLATIONS["CPC_Ench_Control"] = T(
    "付呪のチャージ消費を制御する", "마법부여 충전 소모 제어", "控制附魔的充能消耗",
    "Управлять расходом заряда зачарования", "Kosten der Verzauberungsladung steuern",
    "Contrôler le coût de charge des enchantements", "Controlar el coste de carga del encantamiento",
    "Controlla il costo di carica dell'ammaliamento", "Kontroluj koszt ładunku zaklęcia",
    "Řídit spotřebu náboje očarování")
TRANSLATIONS["CPC_Ench_HelpControl"] = T(
    "既定はオフです。オフの間、この MOD は導入時の値に戻したうえで、そのままにします。",
    "기본값은 꺼짐입니다. 꺼져 있는 동안 이 모드는 설치 당시의 값으로 되돌려 놓고 그대로 둡니다.",
    "默认关闭。关闭时本模组会恢复你安装时的数值并不再干预。",
    "По умолчанию выключено. Пока выключено, мод возвращает значения, с которыми пришла установка, и не трогает их.",
    "Standardmäßig aus. Solange es aus ist, stellt dieser Mod die Werte wieder her, mit denen deine Installation kam, und lässt sie in Ruhe.",
    "Désactivé par défaut. Tant que c'est désactivé, ce mod restaure les valeurs livrées avec votre installation et les laisse tranquilles.",
    "Desactivado por defecto. Mientras lo esté, este mod restaura los valores con los que vino tu instalación y los deja en paz.",
    "Disattivato per impostazione predefinita. Finché è spento, questa mod ripristina i valori con cui è arrivata la tua installazione e li lascia stare.",
    "Domyślnie wyłączone. Gdy jest wyłączone, mod przywraca wartości, z którymi przyszła twoja instalacja, i ich nie rusza.",
    "Ve výchozím stavu vypnuto. Dokud je vypnuto, mod obnoví hodnoty, se kterými tvá instalace přišla, a nechá je být.")
TRANSLATIONS["CPC_Ench_Base"] = T(
    "消費の基本値", "소모 기본값", "消耗基础值", "База стоимости", "Kostenbasis",
    "Base du coût", "Base del coste", "Base del costo", "Baza kosztu", "Základ ceny")
TRANSLATIONS["CPC_Ench_Scale"] = T(
    "消費のスケール", "소모 배율", "消耗缩放", "Масштаб стоимости", "Kostenskalierung",
    "Échelle du coût", "Escala del coste", "Scala del costo", "Skala kosztu", "Škála ceny")
TRANSLATIONS["CPC_Ench_Mult"] = T(
    "消費の倍率", "소모 배수", "消耗倍率", "Множитель стоимости", "Kostenmultiplikator",
    "Multiplicateur du coût", "Multiplicador del coste", "Moltiplicatore del costo",
    "Mnożnik kosztu", "Násobitel ceny")
TRANSLATIONS["CPC_Ench_Exponent"] = T(
    "消費の指数", "소모 지수", "消耗指数", "Показатель степени стоимости", "Kostenexponent",
    "Exposant du coût", "Exponente del coste", "Esponente del costo", "Wykładnik kosztu", "Exponent ceny")
TRANSLATIONS["CPC_Ench_HelpValues"] = T(
    "値を下げるほど、付呪アイテムの使用が安くなります。スキルが 100 を超えたときに暴走するのは指数です。",
    "값을 낮출수록 마법부여 아이템을 쓰는 비용이 싸집니다. 기술이 100을 넘을 때 폭주하는 것은 지수입니다.",
    "数值越低，附魔物品用起来越便宜。当技能超过 100 时失控的正是指数项。",
    "Чем ниже значения, тем дешевле пользоваться зачарованным предметом. Именно показатель степени идёт вразнос, когда навык переваливает за 100.",
    "Niedrigere Werte machen einen verzauberten Gegenstand billiger in der Benutzung. Der Exponent ist der, der davonläuft, wenn die Fertigkeit über 100 klettert.",
    "Des valeurs plus basses rendent un objet enchanté moins coûteux à utiliser. C'est l'exposant qui s'emballe quand la compétence dépasse 100.",
    "Valores más bajos abaratan el uso de un objeto encantado. El exponente es el que se dispara cuando la habilidad pasa de 100.",
    "Valori più bassi rendono un oggetto ammaliato più economico da usare. È l'esponente a scappare via quando l'abilità supera 100.",
    "Niższe wartości sprawiają, że zaklęty przedmiot jest tańszy w użyciu. To wykładnik ucieka, gdy umiejętność przekroczy 100.",
    "Nižší hodnoty zlevňují používání očarovaného předmětu. Je to exponent, který se utrhne, když dovednost přeleze 100.")
TRANSLATIONS["CPC_Ench_SecUsing"] = T(
    "ゲームが使っている値", "게임이 쓰는 값", "游戏正在使用的数值", "Что использует игра",
    "Was das Spiel verwendet", "Ce que le jeu utilise", "Lo que usa el juego",
    "Cosa sta usando il gioco", "Czego używa gra", "Co hra používá")
TRANSLATIONS["CPC_Ench_NotOnRuntime"] = T(
    "%s - このランタイムには存在しません", "%s - 이 런타임에는 없습니다", "%s - 此运行时下不存在",
    "%s - нет на этой версии игры", "%s - auf dieser Laufzeit nicht vorhanden",
    "%s - absent de cette version du jeu", "%s: no existe en esta versión del juego",
    "%s - non presente su questa runtime", "%s - brak na tej wersji gry",
    "%s - na této verzi hry není")
TRANSLATIONS["CPC_Ench_ValueLine"] = T(
    "%s = %.3f   (この環境: %.3f)", "%s = %.3f   (이 설치: %.3f)", "%s = %.3f   （本次安装：%.3f）",
    "%s = %.3f   (эта установка: %.3f)", "%s = %.3f   (diese Installation: %.3f)",
    "%s = %.3f   (cette installation : %.3f)", "%s = %.3f   (esta instalación: %.3f)",
    "%s = %.3f   (questa installazione: %.3f)", "%s = %.3f   (ta instalacja: %.3f)",
    "%s = %.3f   (tato instalace: %.3f)")

# --- Patches -------------------------------------------------------------------------------------
TRANSLATIONS["CPC_Pat_Intro"] = T(
    "この MOD が導入する各エンジンパッチと、その変更内容です。有効でないパッチは不具合ではありません - ゲームのその部分が、この MOD がない場合とまったく同じに振る舞うというだけです。",
    "이 모드가 설치하는 각 엔진 패치와 그 변경 내용입니다. 활성화되지 않은 패치는 결함이 아닙니다 - 게임의 그 부분이 이 모드가 없을 때와 똑같이 동작할 뿐입니다.",
    "本模组安装的每一项引擎补丁及其改动。补丁未启用并不是故障 - 只是游戏的那一部分表现得就像没有本模组一样。",
    "Каждый движковый патч этого мода и что он меняет. Неактивный патч - не ошибка: эта часть игры просто ведёт себя так, как вела бы без мода.",
    "Jeder Engine-Patch, den dieser Mod installiert, und was er ändert. Ein nicht aktiver Patch ist kein Fehler - dieser Teil des Spiels verhält sich dann einfach so, wie er es ohne diesen Mod täte.",
    "Chaque correctif moteur installé par ce mod, et ce qu'il change. Un correctif inactif n'est pas un défaut : cette partie du jeu se comporte simplement comme sans ce mod.",
    "Cada parche de motor que instala este mod y lo que cambia. Un parche que no está activo no es un fallo: esa parte del juego simplemente se comporta como lo haría sin este mod.",
    "Ogni patch al motore installata da questa mod e cosa cambia. Una patch non attiva non è un difetto - quella parte del gioco si comporta semplicemente come farebbe senza questa mod.",
    "Każda łatka silnika instalowana przez ten mod i to, co zmienia. Nieaktywna łatka nie jest usterką - ta część gry po prostu zachowuje się tak, jak bez tego moda.",
    "Každá záplata enginu, kterou tento mod instaluje, a co mění. Neaktivní záplata není chyba - ta část hry se prostě chová tak, jak by se chovala bez tohoto modu.")
TRANSLATIONS["CPC_Pat_SecWhatElse"] = T(
    "他に導入されているもの", "그 밖에 설치된 것", "还安装了什么", "Что ещё установлено",
    "Was sonst installiert ist", "Ce qui est installé par ailleurs", "Qué más está instalado",
    "Cos'altro è installato", "Co jeszcze jest zainstalowane", "Co dalšího je nainstalováno")
TRANSLATIONS["CPC_Pat_DetectedIntro"] = T(
    "読み込み時に検出します。ここで見つかった衝突は「こちらの」機能を切ります - 他 MOD の機能は決して切りません - そして、どのスイッチも上書きはあなたの自由です。",
    "로드 시에 감지합니다. 여기서 발견된 충돌은 '우리' 기능을 끕니다 - 다른 모드의 기능은 결코 끄지 않습니다 - 그리고 모든 스위치는 여전히 당신이 덮어쓸 수 있습니다.",
    "在载入时检测。这里发现的冲突只会关闭“我们的”功能 - 绝不会关闭别的模组的 - 而且每一个开关都仍由你决定是否覆盖。",
    "Определяется при загрузке. Найденный здесь конфликт выключает НАШУ возможность - никогда чужую - и любой переключатель остаётся вашим, чтобы его переопределить.",
    "Beim Laden erkannt. Ein hier gefundener Konflikt schaltet UNSERE Funktion ab - nie die eines anderen Mods - und jeder Schalter bleibt deiner, um ihn zu überstimmen.",
    "Détecté au chargement. Un conflit trouvé ici désactive NOTRE fonction - jamais celle d'un autre mod - et chaque interrupteur reste le vôtre à outrepasser.",
    "Se detecta al cargar. Un conflicto encontrado aquí apaga NUESTRA función, nunca la de otro mod, y cada interruptor sigue siendo tuyo para anularlo.",
    "Rilevato al caricamento. Un conflitto trovato qui spegne LA NOSTRA funzione - mai quella di un'altra mod - e ogni interruttore resta tuo da scavalcare.",
    "Wykrywane przy wczytaniu. Konflikt znaleziony tutaj wyłącza NASZĄ funkcję - nigdy cudzą - a każdy przełącznik nadal należy do ciebie.",
    "Zjištěno při načtení. Konflikt nalezený zde vypne NAŠI funkci - nikdy funkci jiného modu - a každý přepínač zůstává tvůj, abys ho přebil.")
TRANSLATIONS["CPC_Pat_Installed"] = T(
    "%s: 導入済み", "%s: 설치됨", "%s：已安装", "%s: установлен", "%s: installiert",
    "%s : installé", "%s: instalado", "%s: installata", "%s: zainstalowany", "%s: nainstalováno")
TRANSLATIONS["CPC_Pat_NotInstalled"] = T(
    "%s: 未導入", "%s: 설치되지 않음", "%s：未安装", "%s: не установлен", "%s: nicht installiert",
    "%s : non installé", "%s: no instalado", "%s: non installata", "%s: niezainstalowany",
    "%s: nenainstalováno")
TRANSLATIONS["CPC_Pat_SecEnginePatches"] = T(
    "エンジンパッチ", "엔진 패치", "引擎补丁", "Движковые патчи", "Engine-Patches",
    "Correctifs moteur", "Parches de motor", "Patch al motore", "Łatki silnika", "Záplaty enginu")
TRANSLATIONS["CPC_Pat_NoGroups"] = T(
    "このビルドには登録されたパッチグループがありません。",
    "이 빌드에는 등록된 패치 그룹이 없습니다.",
    "本版本中没有注册任何补丁组。",
    "В этой сборке не зарегистрировано ни одной группы патчей.",
    "In diesem Build sind keine Patch-Gruppen registriert.",
    "Aucun groupe de correctifs n'est enregistré dans cette version.",
    "No hay grupos de parches registrados en esta versión.",
    "Nessun gruppo di patch è registrato in questa build.",
    "W tej wersji nie zarejestrowano żadnych grup łatek.",
    "V tomto sestavení nejsou registrovány žádné skupiny záplat.")
TRANSLATIONS["CPC_Pat_Active"] = T(
    "有効", "활성", "已启用", "Активен", "Aktiv", "Actif", "Activo", "Attiva", "Aktywna", "Aktivní")
TRANSLATIONS["CPC_Pat_NotActive"] = T(
    "無効", "비활성", "未启用", "Не активен", "Nicht aktiv", "Inactif", "No activo",
    "Non attiva", "Nieaktywna", "Neaktivní")
TRANSLATIONS["CPC_Pat_Touches"] = T(
    "変更対象: %s", "변경 대상: %s", "影响：%s", "Затрагивает: %s", "Betrifft: %s",
    "Touche : %s", "Toca: %s", "Tocca: %s", "Dotyka: %s", "Dotýká se: %s")
TRANSLATIONS["CPC_Pat_Why"] = T(
    "理由: %s", "이유: %s", "原因：%s", "Почему: %s", "Warum: %s",
    "Pourquoi : %s", "Por qué: %s", "Perché: %s", "Dlaczego: %s", "Proč: %s")

# --- Debug ---------------------------------------------------------------------------------------
TRANSLATIONS["CPC_Dbg_Sec"] = T(
    "デバッグ", "디버그", "调试", "Отладка", "Debug", "Débogage", "Depuración",
    "Debug", "Debugowanie", "Ladění")
TRANSLATIONS["CPC_Dbg_LogLevel"] = T(
    "ログレベル", "로그 수준", "日志级别", "Уровень журнала", "Protokollstufe",
    "Niveau de journal", "Nivel de registro", "Livello di log", "Poziom dziennika", "Úroveň protokolu")
TRANSLATIONS["CPC_Dbg_HelpLogLevel"] = T(
    "すぐに適用されます。ログは Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log にあります。",
    "즉시 적용됩니다. 로그는 Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log 에 있습니다.",
    "立即生效。日志位于 Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log。",
    "Применяется сразу. Журнал лежит в Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.",
    "Wirkt sofort. Das Protokoll liegt unter Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.",
    "S'applique immédiatement. Le journal se trouve dans Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.",
    "Se aplica de inmediato. El registro está en Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.",
    "Si applica subito. Il log si trova in Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.",
    "Działa od razu. Dziennik znajduje się w Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.",
    "Projeví se ihned. Protokol je v Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.")
TRANSLATIONS["CPC_Dbg_Applications"] = T(
    "レベル費用をこのセッションで %llu 回適用しました",
    "이번 세션에서 레벨 비용을 %llu 회 적용했습니다",
    "本次会话中已应用等级花费 %llu 次",
    "Стоимость уровня применена %llu раз(а) за эту сессию",
    "Stufenkosten in dieser Sitzung %llu Mal angewendet",
    "Coût de niveau appliqué %llu fois durant cette session",
    "Coste de nivel aplicado %llu vez(ces) en esta sesión",
    "Costo del livello applicato %llu volta/e in questa sessione",
    "Koszt poziomu zastosowany %llu raz(y) w tej sesji",
    "Cena úrovně použita %llu krát v této relaci")
TRANSLATIONS["CPC_Dbg_LiveSettings"] = T(
    "現在のゲーム設定: 基本 %.1f、レベルごと %.1f",
    "현재 게임 설정: 기본 %.1f, 레벨당 %.1f",
    "当前游戏设置：基础 %.1f，每级 %.1f",
    "Текущие настройки игры: база %.1f, за уровень %.1f",
    "Aktuelle Spieleinstellungen: Basis %.1f, pro Stufe %.1f",
    "Paramètres actuels du jeu : base %.1f, par niveau %.1f",
    "Ajustes actuales del juego: base %.1f, por nivel %.1f",
    "Impostazioni attuali del gioco: base %.1f, per livello %.1f",
    "Bieżące ustawienia gry: baza %.1f, na poziom %.1f",
    "Aktuální nastavení hry: základ %.1f, za úroveň %.1f")

# --- the label tables ----------------------------------------------------------------------------
TRANSLATIONS["CPC_Log_Trace"] = T(
    "Trace", "Trace", "Trace", "Trace", "Trace", "Trace", "Trace", "Trace", "Trace", "Trace")
TRANSLATIONS["CPC_Log_Debug"] = T(
    "Debug", "Debug", "Debug", "Debug", "Debug", "Debug", "Debug", "Debug", "Debug", "Debug")
TRANSLATIONS["CPC_Log_Info"] = T(
    "Info", "Info", "Info", "Info", "Info", "Info", "Info", "Info", "Info", "Info")
TRANSLATIONS["CPC_Log_Warning"] = T(
    "Warning", "Warning", "Warning", "Warning", "Warning", "Warning", "Warning", "Warning", "Warning", "Warning")
TRANSLATIONS["CPC_Log_Error"] = T(
    "Error", "Error", "Error", "Error", "Error", "Error", "Error", "Error", "Error", "Error")
TRANSLATIONS["CPC_Log_Critical"] = T(
    "Critical", "Critical", "Critical", "Critical", "Critical", "Critical", "Critical", "Critical", "Critical", "Critical")
TRANSLATIONS["CPC_Log_Off"] = T(
    "Off", "Off", "Off", "Off", "Off", "Off", "Off", "Off", "Off", "Off")

TRANSLATIONS["CPC_Skill_OneHanded"] = SWF["skOne"]
TRANSLATIONS["CPC_Skill_TwoHanded"] = SWF["skTwo"]
TRANSLATIONS["CPC_Skill_Archery"] = SWF["skArch"]
TRANSLATIONS["CPC_Skill_Block"] = SWF["skBlock"]
TRANSLATIONS["CPC_Skill_Smithing"] = SWF["skSmith"]
TRANSLATIONS["CPC_Skill_HeavyArmor"] = SWF["skHeavy"]
TRANSLATIONS["CPC_Skill_LightArmor"] = SWF["skLight"]
TRANSLATIONS["CPC_Skill_Pickpocket"] = SWF["skPick"]
TRANSLATIONS["CPC_Skill_Lockpicking"] = SWF["skLock"]
TRANSLATIONS["CPC_Skill_Sneak"] = SWF["skSneak"]
TRANSLATIONS["CPC_Skill_Alchemy"] = SWF["skAlch"]
TRANSLATIONS["CPC_Skill_Speech"] = SWF["skSpeech"]
TRANSLATIONS["CPC_Skill_Alteration"] = SWF["skAlter"]
TRANSLATIONS["CPC_Skill_Conjuration"] = SWF["skConj"]
TRANSLATIONS["CPC_Skill_Destruction"] = SWF["skDestr"]
TRANSLATIONS["CPC_Skill_Illusion"] = SWF["skIll"]
TRANSLATIONS["CPC_Skill_Restoration"] = SWF["skRes"]
TRANSLATIONS["CPC_Skill_Enchanting"] = SWF["skEnch"]

TRANSLATIONS["CPC_Attribute_Health"] = T(
    "体力", "체력", "生命", "Здоровье", "Gesundheit", "Santé", "Salud", "Salute", "Zdrowie", "Zdraví")
TRANSLATIONS["CPC_Attribute_Magicka"] = T(
    "マジカ", "매지카", "法力", "Магия", "Magicka", "Magie", "Magia", "Magicka", "Magia", "Magicka")
TRANSLATIONS["CPC_Attribute_Stamina"] = T(
    "スタミナ", "스태미나", "体力", "Запас сил", "Ausdauer", "Vigueur", "Aguante", "Vigore",
    "Kondycja", "Výdrž")

TRANSLATIONS["CPC_Difficulty_Novice"] = T(
    "初心者", "초보자", "新手", "Новичок", "Novize", "Novice", "Novato", "Novizio", "Nowicjusz", "Nováček")
TRANSLATIONS["CPC_Difficulty_Apprentice"] = T(
    "見習い", "견습", "学徒", "Ученик", "Lehrling", "Apprenti", "Aprendiz", "Apprendista", "Uczeń", "Učeň")
TRANSLATIONS["CPC_Difficulty_Adept"] = T(
    "慣れた者", "숙련", "熟练", "Адепт", "Adept", "Adepte", "Adepto", "Adepto", "Adept", "Adept")
TRANSLATIONS["CPC_Difficulty_Expert"] = T(
    "熟練者", "전문가", "专家", "Эксперт", "Experte", "Expert", "Experto", "Esperto", "Ekspert", "Expert")
TRANSLATIONS["CPC_Difficulty_Master"] = T(
    "達人", "달인", "大师", "Мастер", "Meister", "Maître", "Maestro", "Maestro", "Mistrz", "Mistr")
TRANSLATIONS["CPC_Difficulty_Legendary"] = T(
    "伝説", "전설", "传奇", "Легендарный", "Legendär", "Légendaire", "Legendario", "Leggendario",
    "Legendarny", "Legendární")
TRANSLATIONS["CPC_Difficulty_NoneShort"] = T(
    "なし", "없음", "无", "нет", "keiner", "aucune", "ninguna", "nessuna", "brak", "žádná")

TRANSLATIONS["CPC_Regen_HealthRate"] = T(
    "戦闘中の体力再生率", "전투 중 체력 재생률", "战斗中生命回复速率",
    "Скорость регенерации здоровья в бою", "Gesundheitsregeneration im Kampf",
    "Régénération de santé en combat", "Tasa de regeneración de salud en combate",
    "Rigenerazione salute in combattimento", "Tempo regeneracji zdrowia w walce",
    "Rychlost regenerace zdraví v boji")
TRANSLATIONS["CPC_Regen_MagickaRate"] = T(
    "戦闘中のマジカ再生率", "전투 중 매지카 재생률", "战斗中法力回复速率",
    "Скорость регенерации магии в бою", "Magickaregeneration im Kampf",
    "Régénération de magie en combat", "Tasa de regeneración de magia en combate",
    "Rigenerazione magicka in combattimento", "Tempo regeneracji magii w walce",
    "Rychlost regenerace magicky v boji")
TRANSLATIONS["CPC_Regen_StaminaRate"] = T(
    "戦闘中のスタミナ再生率", "전투 중 스태미나 재생률", "战斗中体力回复速率",
    "Скорость регенерации запаса сил в бою", "Ausdauerregeneration im Kampf",
    "Régénération de vigueur en combat", "Tasa de regeneración de aguante en combate",
    "Rigenerazione vigore in combattimento", "Tempo regeneracji kondycji w walce",
    "Rychlost regenerace výdrže v boji")
TRANSLATIONS["CPC_Regen_HealthDelay"] = T(
    "被弾後の体力再生の遅延 (秒)", "피해 후 체력 재생 지연 (초)", "受伤后生命回复延迟（秒）",
    "Задержка регенерации здоровья после урона (с)", "Verzögerung der Gesundheitsregeneration nach Schaden (s)",
    "Délai de régénération de santé après dégâts (s)", "Retardo de regeneración de salud tras el daño (s)",
    "Ritardo di rigenerazione salute dopo il danno (s)", "Opóźnienie regeneracji zdrowia po obrażeniach (s)",
    "Zpoždění regenerace zdraví po poškození (s)")
TRANSLATIONS["CPC_Regen_MagickaDelay"] = T(
    "被弾後のマジカ再生の遅延 (秒)", "피해 후 매지카 재생 지연 (초)", "受伤后法力回复延迟（秒）",
    "Задержка регенерации магии после урона (с)", "Verzögerung der Magickaregeneration nach Schaden (s)",
    "Délai de régénération de magie après dégâts (s)", "Retardo de regeneración de magia tras el daño (s)",
    "Ritardo di rigenerazione magicka dopo il danno (s)", "Opóźnienie regeneracji magii po obrażeniach (s)",
    "Zpoždění regenerace magicky po poškození (s)")
TRANSLATIONS["CPC_Regen_StaminaDelay"] = T(
    "被弾後のスタミナ再生の遅延 (秒)", "피해 후 스태미나 재생 지연 (초)", "受伤后体力回复延迟（秒）",
    "Задержка регенерации запаса сил после урона (с)", "Verzögerung der Ausdauerregeneration nach Schaden (s)",
    "Délai de régénération de vigueur après dégâts (s)", "Retardo de regeneración de aguante tras el daño (s)",
    "Ritardo di rigenerazione vigore dopo il danno (s)", "Opóźnienie regeneracji kondycji po obrażeniach (s)",
    "Zpoždění regenerace výdrže po poškození (s)")
TRANSLATIONS["CPC_Regen_DamagedDelay"] = T(
    "能力値が減ったときの遅延 (秒)", "능력치가 깎였을 때의 지연 (초)", "属性受损后的延迟（秒）",
    "Задержка при повреждённом атрибуте (с)", "Verzögerung bei geschädigtem Attribut (s)",
    "Délai d'attribut endommagé (s)", "Retardo de atributo dañado (s)",
    "Ritardo per attributo danneggiato (s)", "Opóźnienie uszkodzonego atrybutu (s)",
    "Zpoždění poškozeného atributu (s)")
TRANSLATIONS["CPC_Global_HealthCeiling"] = T(
    "体力の遅延の上限 (秒)", "체력 지연 상한 (초)", "生命延迟上限（秒）",
    "Потолок задержки здоровья (с)", "Obergrenze der Gesundheitsverzögerung (s)",
    "Plafond du délai de santé (s)", "Tope del retardo de salud (s)",
    "Limite del ritardo salute (s)", "Górny limit opóźnienia zdrowia (s)",
    "Strop zpoždění zdraví (s)")
TRANSLATIONS["CPC_Global_MagickaCeiling"] = T(
    "マジカの遅延の上限 (秒)", "매지카 지연 상한 (초)", "法力延迟上限（秒）",
    "Потолок задержки магии (с)", "Obergrenze der Magickaverzögerung (s)",
    "Plafond du délai de magie (s)", "Tope del retardo de magia (s)",
    "Limite del ritardo magicka (s)", "Górny limit opóźnienia magii (s)",
    "Strop zpoždění magicky (s)")
TRANSLATIONS["CPC_Global_StaminaCeiling"] = T(
    "スタミナの遅延の上限 (秒)", "스태미나 지연 상한 (초)", "体力延迟上限（秒）",
    "Потолок задержки запаса сил (с)", "Obergrenze der Ausdauerverzögerung (s)",
    "Plafond du délai de vigueur (s)", "Tope del retardo de aguante (s)",
    "Limite del ritardo vigore (s)", "Górny limit opóźnienia kondycji (s)",
    "Strop zpoždění výdrže (s)")
TRANSLATIONS["CPC_Global_OutOfBreath"] = T(
    "息切れ時のスタミナ遅延 (秒)", "숨이 찼을 때의 스태미나 지연 (초)", "力竭时的体力延迟（秒）",
    "Задержка запаса сил при одышке (с)", "Ausdauerverzögerung bei Atemnot (s)",
    "Délai de vigueur à bout de souffle (s)", "Retardo de aguante sin aliento (s)",
    "Ritardo vigore col fiato corto (s)", "Opóźnienie kondycji przy zadyszce (s)",
    "Zpoždění výdrže při vyčerpání dechu (s)")
TRANSLATIONS["CPC_Global_DownedEssential"] = T(
    "戦闘不能になった重要 NPC の再生率", "쓰러진 필수 NPC의 재생률", "倒地要角的回复速率",
    "Скорость регенерации павшего важного NPC", "Regenerationsrate eines niedergestreckten essenziellen NSC",
    "Régénération d'un PNJ essentiel à terre", "Tasa de regeneración de un PNJ esencial abatido",
    "Rigenerazione di un PNG essenziale a terra", "Tempo regeneracji powalonego istotnego NPC",
    "Rychlost regenerace sraženého nezbytného NPC")
TRANSLATIONS["CPC_Stat_TierBelow25"] = T(
    "25 未満", "25 미만", "低于 25", "Ниже 25", "Unter 25", "Moins de 25", "Menos de 25",
    "Sotto 25", "Poniżej 25", "Pod 25")
TRANSLATIONS["CPC_Stat_Tier25"] = T(
    "25 から 49", "25에서 49", "25 到 49", "От 25 до 49", "25 bis 49", "25 à 49", "De 25 a 49",
    "Da 25 a 49", "Od 25 do 49", "25 až 49")
TRANSLATIONS["CPC_Stat_Tier50"] = T(
    "50 から 74", "50에서 74", "50 到 74", "От 50 до 74", "50 bis 74", "50 à 74", "De 50 a 74",
    "Da 50 a 74", "Od 50 do 74", "50 až 74")
TRANSLATIONS["CPC_Stat_Tier75"] = T(
    "75 以上", "75 이상", "75 及以上", "75 и выше", "75 und höher", "75 et plus", "75 y más",
    "75 e oltre", "75 i więcej", "75 a výše")


# ================================================================================================
# 3) Writing the eleven files: the SWF set first, then the CPC_ set, in one file per language.
# ================================================================================================
def write_file(path, order, records):
    lines = []
    for key in order:
        lines.append("$" + key + "\t" + records[key].replace("\n", "\\n"))
    body = "\r\n".join(lines) + "\r\n"
    with io.open(path, "wb") as f:
        f.write(b"\xff\xfe")
        f.write(body.encode("utf-16-le"))


def main():
    page_order, english = read_keys()
    print("source/UI.cpp: {} page keys".format(len(page_order)))

    missing = [k for k in page_order if k not in TRANSLATIONS]
    extra = [k for k in TRANSLATIONS if k not in english]
    if missing:
        raise SystemExit("no translations held for {} key(s): {}".format(len(missing), ", ".join(missing)))
    if extra:
        raise SystemExit("translations held for {} key(s) the source does not use: {}".format(len(extra), ", ".join(extra)))

    swf_missing = [k for k in SWF_ORDER if k not in SWF_ENGLISH or k not in SWF_RUSSIAN or k not in SWF]
    if swf_missing:
        raise SystemExit("SWF set incomplete for: {}".format(", ".join(swf_missing)))

    order = SWF_ORDER + page_order
    out_dir = os.path.join(REPO, "dist", "Interface", "Translations")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    for lang in LANGS:
        records = {}
        for k in SWF_ORDER:
            records[k] = SWF_ENGLISH[k] if lang == "english" else SWF[k][lang]
        for k in page_order:
            records[k] = english[k] if lang == "english" else TRANSLATIONS[k][lang]
        path = os.path.join(out_dir, "{}_{}.txt".format(STEM, lang))
        write_file(path, order, records)
        print("  {:9s} {:3d} keys ({} SWF + {} page) -> {}".format(
            lang, len(order), len(SWF_ORDER), len(page_order), os.path.basename(path)))


if __name__ == "__main__":
    main()
