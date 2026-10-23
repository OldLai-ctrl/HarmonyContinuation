# v0.10.0-dev.2 installer upgrade compatibility

## Root cause and source evidence

The failing predicate is `KnownBundledFactory` in `packaging/UnifiedSetup.iss`.
It examines `%CommonProgramFiles%\VST3\HarmonyContinuation.vst3\Contents\Resources\factory.db`, not LocalAppData user.db or the active ProgramData Factory.
Dev.1 accepts only its V4 payload SHA-256 and an optional `factory-v2.db` build artifact hash; no V3 reference is included. The optional V2 hash can also be empty in a clean V4 build. Therefore a trusted RC/older bundled V3 copy incorrectly blocks a combined engine+Library upgrade.

Actual device Setup logs on 2026-10-10 at 21:12:50 / 21:13:09 record the reported error. An earlier log at 00:23:06 records the 0.9.0-rc.1 installer writing that bundled path. By inspection in this task, the bundled file and installed plugin binary are absent; its hash at the time of failure cannot be recovered from those logs and must not be invented. The later 21:15:22 attempt fails at a distinct explicit-removal ownership guard, not this predicate; that guard remains enabled.

Corroborated historical artifacts:

| Object | SHA-256 | Identity evidence |
| --- | --- | --- |
| Retained 0.9.0-rc.1 Setup | 5a8fbe9a1842b297b65420077239fb3ec569c9227a1b6faaa43aec0861b6b751 | Local Setup file referenced by actual installation log; extracted without executing production install |
| Its bundled and extracted Factory, and installed ProgramData Library 3 | 5ee13340cc86d4743d2682b9714d58f9bc85076a60336640f605f04d2d9d7693 | Complete schema/metadata/payload match to the 629-entry reference generated from official v0.9.0 source; data and db_compiler unchanged against tag ddc4f89 |
| Archived RC1 Library 2 and installed backup | 9b1fae16302d13a97e1f694ad7147be08920078e6c16057586aaa34e28fbe076 | Original RC1 SHA256SUMS and BUILD.txt (52ab9ec); shipped stage DB and preserved installed plugin backup agree |
| Current repository Library 2 reference | 4c9d5cf94783de2bc063795643039fd7f3f9321ba2ef779e5fcc9f07cd9d015e | Compiled from unchanged repository data/factory |
| V4 payload | 74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534 | Identical to retained dev.1 extracted DB; Library 4 / Schema 2 / 657 |

Manual replacement can leave the same bundled residue; retained logs prove RC1 installation wrote it, but do not prove an RC4 manual replacement occurred on this device. Official 0.9.0 unified staging excludes the bundled fallback. Pure formal upgrade and trusted residue upgrade are tested separately.

## Fix and protection boundaries

- `library_manager verify-factory` opens read-only and compares all allowed schema objects, user_version/application_id, every metadata value and every raw progression ID/payload byte to packaged official V2/V3/V4 references. It rejects additional tables/views/triggers and bounds input size. Metadata, version and entry count alone never establish identity. SQLite physical layout may differ while full content matches.
- An exact corroborated RC1 V2 hash also authorizes migration; a changed RC1 file is rejected unless its entire content independently matches an official reference.
- Before removing recognized bundled residue, recheck its hash/sidecars, create a permanent hash-addressed backup under `%ProgramData%\HarmonyContinuation\Backups\FactoryMigration`, and verify the copied hash. A pre-existing different backup is not overwritten. Failure prevents replacement and restores transactional files.
- Unknown/modified files remain in place. Chinese and English rejection messages show conflict path, actual SHA-256, readable version/schema/count (explicitly unverified), reason and backup/recovery instructions. There is no forced-adoption or silent-backup consent switch.
- The selected engine and V4 payload must match; a retained 0.9.0 engine cannot accept library-only V4. Deselecting an update component preserves it. Component state now keeps LibraryVersion and does not overwrite LibraryHash on plugin-only updates; removal/uninstall addresses the owned version rather than the incoming payload version.
- Core music code, Factory sources, Schema and runtime algorithms are unchanged. Only version identity requires a new dev.2 plugin build. No production installer or uninstaller is executed in this task.

## Focused verification

`tools/test-dev2-upgrade.ps1` compiles test installers with workspace-owned paths, lowest privileges and short unique HKCU identities. Old 0.9.0 plugin and installer behavior are rebuilt from official tag ddc4f89; the historical installer template is changed only to parameterize its isolated test suffix. This is a tag-reconstructed fixture, not a claim to possess the original published Setup binary.

Initial 413-check run passed: fresh install; official tag fixture upgrade; actual RC bundled V3/plugin replacement and corroborated RC V2 residue; modified valid Factory rejection in both languages; complete-content match with different physical hash; component update/repair/modify/uninstall; permanent backup and personal/history/unmanaged sentinel preservation.

Final build identity, expanded verification and package checksums are recorded in the final delivery section after completion. No full CTest, Validator, DAW matrix or music benchmark is run. Cubase manual acceptance of dev.2 remains pending.
## Final delivery

INSTALLER UPGRADE FIX READY. Product 0.10.0-dev.2 / Release / build commit 533eb463a4e0554f1f66d72775b8d64b949ba898. Later evidence/test-documentation commits do not change binary identity.

Final focused run: 611 checks PASS across fresh, pure official-v09, RC/manual, unknown, official content with changed physical layout, old-engine-only update and failed-backup protection. Additional legacy dual-installer migration: 93 checks PASS. Total 704. All real installer/uninstaller executions use isolated workspace paths and HKCU identities; original old published Setup binary was unavailable, so formal upgrade uses a plugin/Setup rebuilt from the exact v0.9.0 tag. The RC/manual path uses the actual retained RC artifacts.

Final Setup extraction: 16/16 plugin files match the staging ownership manifest; embedded plugin commit is 533eb46; module version 0.10.0-dev.2; extracted DB Library 4 / Schema 2 / 657. V4 Factory SHA remains identical to dev.1. Actual device production Library files/pointer checksums unchanged; personal/favourites/settings/progression/history/unmanaged sentinels preserved in isolation. Permanent backups survive modify/repair/uninstall. Failed existing-backup verification preserves original bytes, engine and component state.

Package: `build-installer/v4-0.10.0-dev.2/HarmonyContinuation-0.10.0-dev.2-Setup.exe`.
Setup SHA-256: `95984545b637e1e70498d8658bc9cd8e5b1f1596a8d8433758fbe4e30bc31ba5`.
`BUILD_INFO.json`, `FILE_MANIFEST.sha256`, `SHA256SUMS.txt` and evidence are alongside the package. Dev.1 download/package is not replaced. User must close hosts and select both components when moving from 0.9.0 to V4. Next step is user reinstall and Cubase manual acceptance only.