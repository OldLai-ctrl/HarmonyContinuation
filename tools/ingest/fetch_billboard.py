import hashlib
import ssl
import sys
import urllib.request
from pathlib import Path

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

TARGETS = [
    ("https://www.dropbox.com/s/o0olz0uwl9z9stb/billboard-2.0-index.csv?dl=1",
     "billboard-index.csv",
     "fc39435098814f1a4f1dd5c95c72aff399b648b78f33a59682924dfb9b5280fb"),
    ("https://www.dropbox.com/s/p4xtixbvt4hw5c6/billboard-2.0-salami_chords.tar.xz?dl=1",
     "billboard-salami.tar.xz",
     "92d5797743f5bbba070fd233b028918707832066340641bd27f435236ee7457e"),
]

out = Path(r"D:\VibeCoding\_hc_evidence\sources")
out.mkdir(parents=True, exist_ok=True)

for url, name, digest in TARGETS:
    target = out / name
    if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == digest:
        print("cached", name, target.stat().st_size)
        continue
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(request, context=ctx, timeout=300) as response:
        data = response.read()
    actual = hashlib.sha256(data).hexdigest()
    print(name, len(data), "sha_ok" if actual == digest else ("SHA_MISMATCH " + actual))
    target.write_bytes(data)
    sys.stdout.flush()
print("done")
