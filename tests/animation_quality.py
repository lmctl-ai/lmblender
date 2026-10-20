#!/usr/bin/env python3
import pathlib, struct, sys, zlib
root=pathlib.Path(sys.argv[1])
def png_rgb(path):
    b=path.read_bytes(); assert b[:8]==b'\x89PNG\r\n\x1a\n'
    pos=8; compressed=bytearray(); width=height=ctype=None
    while pos+12<=len(b):
        n=struct.unpack_from('>I',b,pos)[0]; typ=b[pos+4:pos+8]; d=b[pos+8:pos+8+n]
        assert len(d)==n, 'truncated PNG chunk'
        if typ==b'IHDR': width,height,depth,ctype,_,_,_=struct.unpack('>IIBBBBB',d)
        if typ==b'IDAT': compressed.extend(d)
        pos += n+12
        if typ==b'IEND': break
    assert depth==8 and ctype in (2,6), 'expected RGB/RGBA 8-bit frame'
    channels=4 if ctype==6 else 3; raw=zlib.decompress(compressed); stride=width*channels
    rows=[]; off=0; prior=bytearray(stride)
    for _ in range(height):
        f=raw[off]; off+=1; row=bytearray(raw[off:off+stride]); off+=stride
        for i in range(stride):
            a=row[i-channels] if i>=channels else 0; up=prior[i]; ul=prior[i-channels] if i>=channels else 0
            if f==1: row[i]=(row[i]+a)&255
            elif f==2: row[i]=(row[i]+up)&255
            elif f==3: row[i]=(row[i]+((a+up)//2))&255
            elif f==4:
                p=a+up-ul; pa=abs(p-a); pb=abs(p-up); pc=abs(p-ul); q=a if pa<=pb and pa<=pc else up if pb<=pc else ul
                row[i]=(row[i]+q)&255
            elif f!=0: raise AssertionError('unsupported PNG filter')
        rows.append(row); prior=row
    rgb=bytearray()
    for row in rows:
        for x in range(width): rgb.extend(row[x*channels:x*channels+3])
    return width,height,rgb
w,h,first=png_rgb(root/'tween-0.png'); w2,h2,mid=png_rgb(root/'tween-4.png'); w3,h3,last=png_rgb(root/'tween-8.png')
assert (w,h)==(w2,h2)==(w3,h3)
assert first==last, 'loop closure: frame 0 and 8 should render identically'
delta=sum(abs(a-b) for a,b in zip(first,mid))/(w*h*3)
assert delta>0.5, f'animation delta too small: mean absolute RGB difference {delta:.3f}'
b=(root/'animation-tween.apng').read_bytes()
assert b.startswith(b'\x89PNG\r\n\x1a\n') and b'acTL' in b and b'fcTL' in b and b'fdAT' in b, 'APNG must contain animation control and frame data chunks'
pos=b.index(b'acTL'); assert struct.unpack('>I',b[pos+4:pos+8])[0]==9, 'APNG frame count must be exactly nine'
print(f'animation quality: frame delta {delta:.3f}/255, loop closure exact, APNG has 9 frames')
