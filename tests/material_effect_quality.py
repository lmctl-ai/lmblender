import sys
from png_metrics import decode

def pixels(path):
    w,h,c,rows=decode(path)
    assert (w,h,c) == (640,480,4), f"{path}: expected 640x480 RGBA"
    rgb=[tuple(row[x:x+3]) for row in rows for x in range(0,len(row),c)]
    return [v for pixel in rgb for v in pixel], len(set(rgb))

(a,_),(b,unique)=map(pixels,sys.argv[1:])
mad=sum(abs(x-y) for x,y in zip(a,b))/len(a)
print(f"{sys.argv[2]}: mean absolute pixel delta vs opaque baseline={mad:.3f}/255; {unique} unique RGB colors")
assert unique >= 100, f"{sys.argv[2]}: output is visually flat ({unique} unique RGB colors), object/lighting result absent"
assert mad >= 0.35, f"{sys.argv[2]}: effect is not measurably different from opaque baseline (MAD {mad:.3f})"
if "glass" in sys.argv[2]:
    w,h,c,rows=decode(sys.argv[2]); roi=[tuple(rows[y][x*c:x*c+3]) for y in range(140,350) for x in range(220,420)]
    lum=[.2126*p[0]+.7152*p[1]+.0722*p[2] for p in roi]
    all_rows=decode(sys.argv[2])[3]
    isolated=0;considered=0
    for y in range(141,349):
      for x in range(221,419):
       p=all_rows[y][x*4:x*4+3]
       if min(p)>=245:
        considered+=1
        neighbors=[all_rows[yy][xx*4:xx*4+3] for yy in range(y-1,y+2) for xx in range(x-1,x+2) if (xx,yy)!=(x,y)]
        if all(max(q)<190 for q in neighbors):isolated+=1
    outliers=isolated/(considered or 1)
    core=[rows[y][x*c:x*c+3] for y in range(185,295) for x in range(265,375)]
    core_lum=[.2126*p[0]+.7152*p[1]+.0722*p[2] for p in core]
    core_sd=(sum((v-sum(core_lum)/len(core_lum))**2 for v in core_lum)/len(core_lum))**0.5
    print(f"glass sphere ROI: luminance stddev={(sum((v-sum(lum)/len(lum))**2 for v in lum)/len(lum))**0.5:.2f}; core checker stddev={core_sd:.2f}; isolated bright pixels={outliers:.3%} of near-white pixels")
    assert outliers<0.02, f"glass ROI has excessive isolated fireflies: {outliers:.3%}"
    assert core_sd>12.0, f"glass sphere does not show refracted floor contrast in its center: luminance stddev {core_sd:.2f}"
    assert len(set(roi))>1000, "glass sphere should show a non-uniform refracted checker pattern"
if "volume-fog" in sys.argv[2]:
    w,h,c,rows=decode(sys.argv[2])
    subject=[];blue_haze=0
    for y in range(h):
      for x in range(w):
        p=rows[y][x*c:x*c+3];r,g,b=p
        if r>g*1.12 and r>b*1.1 and r>55: subject.append(.2126*r+.7152*g+.0722*b)
        if 185<x<455 and 80<y<345 and b>r+8 and b>g*0.9: blue_haze+=1
    print(f"fog shell audit: lit red subject pixels={len(subject)}; blue haze pixels={blue_haze}")
    assert len(subject)>1500, f"fog volume obscures the lit subject ({len(subject)} red pixels)"
    assert blue_haze>10000, f"blue scattering haze is not visible ({blue_haze} pixels)"
if "subsurface" in sys.argv[2]:
    w,h,c,rows=decode(sys.argv[2]); roi=[rows[y][x*c:x*c+3] for y in range(130,370) for x in range(220,420)]
    lum=[.2126*p[0]+.7152*p[1]+.0722*p[2] for p in roi]
    span=max(lum)-min(lum)
    sphere=[.2126*p[0]+.7152*p[1]+.0722*p[2] for p in roi if p[0]>p[1]*1.35 and p[0]>p[2]*1.25]
    sphere_span=max(sphere)-min(sphere) if sphere else 0
    print(f"wax sphere ROI luminance span={span:.2f}; orange sphere mask pixels={len(sphere)}; sphere lit-to-shadow span={sphere_span:.2f}")
    assert len(sphere)>5000, f"wax sphere mask is too small to measure ({len(sphere)} pixels)"
    assert sphere_span>25, f"wax sphere lacks a visible light-to-shadow gradient: span={sphere_span:.2f}"
