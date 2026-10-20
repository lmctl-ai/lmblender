#!/usr/bin/env python3
import struct, sys, zlib
from pathlib import Path

def png_rgb(path):
    data=Path(path).read_bytes(); assert data[:8]==b'\x89PNG\r\n\x1a\n', f'{path}: invalid PNG'
    w,h=struct.unpack_from('>II',data,16); color_type=data[25]; bpp=4 if color_type==6 else 3 if color_type==2 else 0
    assert bpp, f'{path}: expected RGB/RGBA PNG, color_type={color_type}'
    pos=8; raw=bytearray()
    while pos<len(data):
        n=struct.unpack_from('>I',data,pos)[0]; typ=data[pos+4:pos+8]; chunk=data[pos+8:pos+8+n]; pos+=12+n
        if typ==b'IDAT': raw.extend(chunk)
    decoded=zlib.decompress(raw); stride=w*bpp; rows=[]; offset=0; prev=bytearray(stride)
    for _ in range(h):
        filt=decoded[offset]; offset+=1; cur=bytearray(decoded[offset:offset+stride]); offset+=stride
        for i in range(stride):
            a=cur[i-bpp] if i>=bpp else 0; b=prev[i]; c=prev[i-bpp] if i>=bpp else 0
            if filt==1: cur[i]=(cur[i]+a)&255
            elif filt==2: cur[i]=(cur[i]+b)&255
            elif filt==3: cur[i]=(cur[i]+((a+b)//2))&255
            elif filt==4:
                p=a+b-c; pa=abs(p-a); pb=abs(p-b); pc=abs(p-c); cur[i]=(cur[i]+(a if pa<=pb and pa<=pc else b if pb<=pc else c))&255
        rows.append(cur); prev=cur
    return w,h,rows,bpp

path=sys.argv[1]; w,h,rows,bpp=png_rgb(path); assert (w,h)==(640,480),f'{path}: expected 640x480, got {w}x{h}'
lum=[]; object_lum=[]; colored=0; xs=[]; ys=[]
for iy,row in enumerate(rows):
    for x in range(0,len(row),bpp):
        r,g,b=row[x:x+3]; y=.2126*r+.7152*g+.0722*b; lum.append(y)
        chroma=max(r,g,b)-min(r,g,b)
        if chroma>25 and y>35: colored+=1; object_lum.append(y)
        if chroma>55 and y>45: xs.append(x//bpp); ys.append(iy)
lum.sort(); p05=lum[int(.05*len(lum))]; p95=lum[int(.95*len(lum))]; mean=sum(lum)/len(lum)
assert colored>15000, f'{path}: insufficient colored object pixels ({colored})'
assert min(xs)>10 and max(xs)<w-11 and min(ys)>10 and max(ys)<h-11, f'{path}: colored subject clipped or too close to frame edge'
assert p95-p05>24, f'{path}: studio lighting has inadequate luminance range ({p95-p05:.1f})'
object_mean=sum(object_lum)/max(1,len(object_lum))
assert object_mean>70, f'{path}: colored subject too dark (mean luminance {object_mean:.1f})'
print(f'{path}: PASS 640x480, colored_pixels={colored}, bbox=({min(xs)},{min(ys)})..({max(xs)},{max(ys)}), subject_mean_luma={object_mean:.1f}, image_p05-p95={p95-p05:.1f}')
