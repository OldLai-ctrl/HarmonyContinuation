# Human continuation ratings

This directory is intentionally empty until a person listens to a candidate in Demo Benchmark mode and presses **Save Rating**. Each saved file is named by benchmark ID and a stable hash of the existing continuation fingerprint. Ratings are never written to `factory.db` or `user.db`.

Schema version 1 stores: `benchmarkId`, `candidateFingerprint`, an embedded `recommendationSnapshot`, five integer scores (`naturalness`, `intentFit`, `rhythmFit`, `distinctiveness`, `usability`) from 1 to 5, `verdict` (`KEEP`, `QUESTIONABLE`, `BAD`), `issueCategory` (`Harmony`, `Matching`, `Ranking`, `Intent`, `Rhythm`, `Diversity`, `Voicing`, `Sound`, `UI`, `Other`), `note`, and `algorithmContext` (`libraryVersion`, `sessionSchemaVersion`, `matchingConfigVersion`, `recommendationConfigVersion`). The file is read back and validated after saving.

Choose `Sound` or `Voicing` when the progression works but the preview timbre or voicing does not. Do not use those ratings to change continuation rankings without listening to the harmony independently.
