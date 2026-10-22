/*
  Q Light Controller
  eosasciiparser.h

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef EOSASCIIPARSER_H
#define EOSASCIIPARSER_H

#include <QString>
#include <QStringList>
#include <QHash>

class Doc;
class EosFixtureMapper;
class EosImportResult;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Streaming reader for the USITT ASCII Text Representation of Lighting
 * Console Data (1992), ETC Eos dialect. See
 * https://support.etcconnect.com/.../Eos_Family_USITT_ASCII_Import for the
 * records Eos emits.
 *
 * Record types handled in v1:
 *   Ident, Manufacturer, Console, Clear, Version   -- metadata (warnings only)
 *   $FixtureList                                   -- personality table
 *   Patch, $Patch                                  -- fixture address + personality
 *   Group, $Preset (dedupe)                        -- fixture groups
 *   Sub                                            -- submaster → Scene
 *   Cue                                            -- cue → Scene
 *   $IntensPalette, $FocusPalette,
 *   $ColorPalette, $BeamPalette                    -- palette shells (values empty)
 *
 * Unknown $-prefixed records are skipped with a warning; the parser must
 * never fail-hard on an unrecognised record (34-year-old format with ongoing
 * vendor extensions).
 *
 * Fixture type resolution (Eos personality → QLC def) is delegated to the
 * EosFixtureMapper so personality-matching UI stays out of the engine.
 *
 * See EOS_IMPORT_DESIGN.md § "USITT ASCII — what to parse" for the full
 * record-by-record contract and known Eos quirks (dual-emitted groups,
 * renumbered subs, flattened cue-part timings).
 */
class EosAsciiParser final
{
public:
    EosAsciiParser();
    ~EosAsciiParser();

    /**
     * Parse the file at @p path and populate @p doc with the fixtures,
     * groups, scenes (one per cue, one per sub) and palette shells it finds.
     * @p mapper resolves each referenced Eos personality to a QLC fixture
     * definition. @p result accumulates counts + warnings.
     *
     * @return true if the file was parseable at all (not: perfectly). On
     *         false the Doc may be partially populated and should be
     *         discarded by the caller.
     */
    bool parse(const QString &path, Doc *doc,
               EosFixtureMapper *mapper, EosImportResult &result);

private:
    // Record handlers filled in during implementation. Deliberately kept out
    // of the header to avoid freezing the Eos-side vocabulary of structs.
};

/** @} */

#endif
