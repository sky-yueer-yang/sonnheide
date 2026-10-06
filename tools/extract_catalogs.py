#!/usr/bin/env python3
"""Extract only facts actually present in the supplied v0.6 Markdown."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/design/Sonnheide_Complete_Design_v0.6.md"


def write(name, value):
    path = ROOT / "data/catalogs" / name
    path.write_bytes((json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))


def main():
    raw = SOURCE.read_bytes()
    text = raw.decode("utf-8")
    provenance = {"source": SOURCE.relative_to(ROOT).as_posix(), "sha256": hashlib.sha256(raw).hexdigest(),
                  "design_version": "0.6", "status": "extracted_definitions_not_implemented_rules"}
    capabilities, families, slots, laws = [], [], [], []
    direction = None
    section = 0
    businesses, commands = [], []
    for line_number, line in enumerate(text.splitlines(), 1):
        chapter = re.match(r"# (\d+) ·", line)
        if chapter:
            section = int(chapter.group(1))
        heading = re.match(r"## (.+) / (.+)", line)
        if heading and section == 25:
            direction = {"zh-CN": heading.group(1), "en": heading.group(2)}
        if not line.startswith("|"):
            continue
        columns = [x.strip() for x in line.strip("|").split("|")]
        if section == 25 and re.fullmatch(r"T\d{3}", columns[0]):
            capabilities.append({"id": columns[0], "name": {"zh-CN": columns[1], "en": columns[2]},
                                 "prerequisite_text": columns[3], "prerequisite_ids": re.findall(r"T\d{3}", columns[3]),
                                 "unlocks": columns[4], "source_line": line_number})
        elif section == 25 and re.fullmatch(r"F\d{4}", columns[0]):
            names = columns[1].split(" / ", 1)
            family = {"id": columns[0], "name": {"zh-CN": names[0], "en": names[1]}, "direction": direction,
                      "prerequisite_ids": re.findall(r"T\d{3}", columns[3]), "slot_ids": [], "source_line": line_number}
            for slot_id, name in re.findall(r"(F\d{4}-S[123]) ([^；]+)", columns[2]):
                family["slot_ids"].append(slot_id)
                slots.append({"id": slot_id, "family_id": family["id"], "name": {"zh-CN": name},
                              "source_line": line_number, "translation_status": "en_pending"})
            families.append(family)
        elif section == 26 and columns[0].startswith("LAW_"):
            names = columns[1].split(" / ", 1)
            laws.append({"id": columns[0], "name": {"zh-CN": names[0], "en": names[1]},
                         "trigger": columns[2], "constraint": columns[3], "source_line": line_number})
        elif section == 26 and len(columns) == 3 and " / " in columns[0]:
            names = columns[0].split(" / ", 1)
            businesses.append({"name": {"zh-CN": names[0], "en": names[1]}, "earliest_stage": columns[1],
                               "capacity_constraints": columns[2], "source_line": line_number})
        elif section == 27 and len(columns) == 4 and re.fullmatch(r"[A-Z][A-Za-z]+", columns[0]):
            commands.append({"kind": columns[0], "payload_requirements": columns[1], "validation_requirements": columns[2],
                             "outcome_rules": columns[3], "source_line": line_number, "implementation_status": "planned"})
    if [len(capabilities), len(families), len(slots), len(laws), len(businesses), len(commands)] != [40, 96, 288, 35, 13, 23]:
        raise ValueError("Design catalog extraction is incomplete; no files updated")
    write("technology.json", {"provenance": provenance, "capabilities": capabilities, "families": families, "slots": slots})
    write("laws.json", {"provenance": provenance, "laws": laws})
    write("businesses.json", {"provenance": provenance, "businesses": businesses})
    write("commands.json", {"provenance": provenance, "commands": commands})
    write("design_source.json", provenance)
    print("Extracted 40 capabilities + 96 families + 288 slots, 35 laws, 13 businesses, 23 command specifications")


if __name__ == "__main__":
    main()
