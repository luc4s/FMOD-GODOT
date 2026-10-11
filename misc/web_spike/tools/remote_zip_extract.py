import sys, struct, urllib.request, zlib
url, member, out = sys.argv[1:4]
def rng(a, b):
    req = urllib.request.Request(url, headers={"Range": f"bytes={a}-{b}"})
    with urllib.request.urlopen(req) as r:
        return r.read(), r.headers.get("Content-Range")
_, cr = rng(0, 0); size = int(cr.split("/")[1])
tail, _ = rng(size - 131072, size - 1)
j = tail.rfind(b"PK\x06\x06")
if j != -1: cd_size, cd_off = struct.unpack("<QQ", tail[j+40:j+56])
else:
    i = tail.rfind(b"PK\x05\x06"); cd_size, cd_off = struct.unpack("<II", tail[i+12:i+20])
cd, _ = rng(cd_off, cd_off + cd_size - 1); p = 0
while cd[p:p+4] == b"PK\x01\x02":
    meth, = struct.unpack("<H", cd[p+10:p+12]); csize, usize = struct.unpack("<II", cd[p+20:p+28])
    n, e, c = struct.unpack("<HHH", cd[p+28:p+34]); off, = struct.unpack("<I", cd[p+42:p+46])
    name = cd[p+46:p+46+n].decode()
    if name == member:
        hdr, _ = rng(off, off + 29); ln, le = struct.unpack("<HH", hdr[26:30])
        data, _ = rng(off + 30 + ln + le, off + 30 + ln + le + csize - 1)
        open(out, "wb").write(zlib.decompress(data, -15) if meth == 8 else data); print("ok", usize); break
    p += 46 + n + e + c
