#!/usr/bin/env python3
"""Download ONNX Runtime (CPU) from NuGet and unpack headers + the native
library for this platform into third_party/onnxruntime/{include,lib}.

Usage: python3 scripts/fetch_onnxruntime.py [linux-x64|win-x64|osx-arm64] [dest]
"""
import io, os, platform, shutil, sys, urllib.request, zipfile

VERSION = "1.31.1"
URL = f"https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/{VERSION}/microsoft.ml.onnxruntime.{VERSION}.nupkg"

def default_rid():
    system = platform.system()
    if system == "Windows":
        return "win-x64"
    if system == "Darwin":
        return "osx-arm64"
    return "linux-x64"

rid = sys.argv[1] if len(sys.argv) > 1 else default_rid()
dest = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "third_party", "onnxruntime")
inc, lib = os.path.join(dest, "include"), os.path.join(dest, "lib")
shutil.rmtree(dest, ignore_errors=True)
os.makedirs(inc), os.makedirs(lib)

print(f"Downloading ONNX Runtime {VERSION} for {rid}…")
data = urllib.request.urlopen(URL, timeout=600).read()
with zipfile.ZipFile(io.BytesIO(data)) as z:
    for name in z.namelist():
        base = os.path.basename(name)
        if not base:
            continue
        if name.startswith("build/native/include/"):
            target = inc
        elif name.startswith(f"runtimes/{rid}/native/"):
            target = lib
        else:
            continue
        with z.open(name) as src, open(os.path.join(target, base), "wb") as out:
            shutil.copyfileobj(src, out)

# The Linux library's soname is libonnxruntime.so.1; provide that name too.
so = os.path.join(lib, "libonnxruntime.so")
if os.path.exists(so):
    shutil.copy(so, os.path.join(lib, "libonnxruntime.so.1"))
print("Installed into", os.path.abspath(dest), "->", sorted(os.listdir(lib)))
