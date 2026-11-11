// MetaHuman Sweat: the sweat field. Body of a material Custom node (float4 output: bead alpha, film alpha, normal delta x, normal delta y).
// Inputs: float2 UV, float4 Region (mask RGBA), float3 Up (world up in tangent space), float Amount, Wet, Cells, Density, Size, Streak, Intensity,
// float3 Axes, float Seed, float GameTime. Written into the wet skin material by the MetaHuman Sweat editor tools; edit here, then run
// Tools > MetaHuman Sweat > Update Sweat Materials.
#define MHS_HASH(P, OUT) { float3 q_ = frac(float3((P).xyx) * float3(0.1031, 0.1030, 0.0973)); q_ += dot(q_, q_.yzx + 33.33); OUT = frac((q_.xxy + q_.yzz) * q_.zyx); }

float amount = saturate(Amount * Intensity);
float wet = saturate(Wet * Intensity);
float zoneBead = saturate(Region.r);
float zoneRun = saturate(Region.g);
float zoneFilm = 0.4 + 0.6 * zoneBead;
// The mask's alpha is the cell-density level: 1 = this part's nominal density, 0.5 = doubled (where the part's UVs are stretched to about twice the
// nominal scale: the face mesh's lower neck and collar), so a bead is the same size on the skin on both sides of the head/body mesh boundary.
// The mask ramps softly between the two levels; inside the ramp no bead or trail is drawn (levelGate), only the sheen, so no bead is cut in half.
float levelT = saturate((1.0 - Region.a) * 2.0);
float levelGate = saturate((abs(levelT - 0.5) - 0.12) / 0.2);
float2 seedOff = float2(Seed * 61.7, Seed * 37.3);
float cellScale = max(Cells, 1.0) * max(Density, 0.05) * (1.0 + step(0.5, levelT));
float2 uvc = UV * cellScale;
// A smooth low-frequency warp bends the lattices (no visible rows of beads). The patch field that makes sweat gather in patches is baked into the
// mask's blue channel from the rest position, so it is the same on both sides of the head/body mesh boundary (a patch field computed from each
// mesh's own UVs stepped there and drew a line across the upper chest).
float2 warp = float2(sin(uvc.y * 0.37 + Seed * 6.28) + 0.35 * sin(uvc.y * 1.13 + uvc.x * 0.21 + 1.7), sin(uvc.x * 0.31 + 2.1 + Seed * 6.28) + 0.35 * sin(uvc.x * 0.97 + uvc.y * 0.17)) * 0.6;
float patch = saturate(Region.b);
// The damp sheen (uneven by the patch field: wet skin shines in patches, pores and hair still read) and the thin film of rain and immersion. Neither
// depends on the cell lattices, so both run unbroken through the level ramp.
float sheen = max(amount * zoneFilm * lerp(0.45, 1.1, patch), wet * (0.35 + 0.65 * zoneFilm) * 0.45);
// Beads smaller than a couple of pixels shimmer under temporal AA: fade them out by the cell's screen footprint
// (cells per pixel) and leave a faint damp sheen in their place.
float foot = max(length(ddx(UV * cellScale)), length(ddy(UV * cellScale)));
float beadFade = 1.0 - smoothstep(0.2, 0.5, foot);
float dropFade = 1.0 - smoothstep(0.2, 0.5, foot * 2.6);

float beadAlpha = 0.0;
float filmAlpha = 0.0;
float2 nDelta = float2(0.0, 0.0);

// 1. Beads: one jittered disc (sometimes two, merged) per cell, growing and multiplying with the sweat.
{
    float grow = saturate(amount * zoneBead * 1.15 * lerp(0.5, 1.2, patch));
    float2 pB = uvc + warp;
    float2 cB = floor(pB);
    float2 fB = pB - cB;
    float3 hA;
    MHS_HASH(cB + seedOff, hA);
    float3 hB;
    MHS_HASH(cB + seedOff + 17.31, hB);
    float present = step(hA.x, 0.5 * pow(grow, 0.8)) * step(0.001, grow);
    float r = min(lerp(0.05, 0.21, grow) * (0.45 + 0.95 * hA.y) * Size, 0.46);
    float2 c1 = r + (1.0 - 2.0 * r) * float2(hA.z, hB.x);
    float pairP = saturate((grow - 0.55) * 2.2) * 0.35;
    float p2 = present * step(hB.y, pairP);
    float r2 = r * (0.55 + 0.4 * hB.z);
    float ang = hB.z * 6.2831853;
    float2 c2 = clamp(c1 + float2(cos(ang), sin(ang)) * (r + r2) * 0.72, float2(r2, r2), float2(1.0 - r2, 1.0 - r2));
    float2 q1 = fB - c1;
    float2 q2 = fB - c2;
    float d1 = length(q1) / r;
    float d2 = length(q2) / r2;
    float a1 = smoothstep(0.0, 0.08, saturate(1.0 - d1 * d1)) * present * beadFade;
    float a2 = smoothstep(0.0, 0.08, saturate(1.0 - d2 * d2)) * p2 * beadFade;
    float beadMain = saturate(a1 + a2 - a1 * a2);
    float halo = (1.0 - smoothstep(1.0, 1.45, d1)) * present * 0.45 * beadFade;
    beadAlpha = max(beadAlpha, beadMain);
    filmAlpha = max(filmAlpha, max(max(halo, beadMain * 0.6), (1.0 - beadFade) * grow * 0.35));
    nDelta += (q1 / r * a1 + q2 / r2 * a2) / max(1.0, a1 + a2) * 0.45;
}

// 2. Streaks: drops that start at FIXED points on the skin and run downhill over time. Nothing here is a lattice rotated by gravity (that swam
// with every small head movement): a streak is anchored at its start point in UV, so gravity (the tangent-space "up" vector) only turns the
// trail about that start point, a lever arm of a few cells. A drop's position along its run depends on GameTime alone and only ever grows
// downhill, so it can never run back up. Each pixel looks up-slope along the downhill axis in six steps and tests the start point of the
// cell it lands in.
{
    float2 g = float2(-Up.x * Axes.x, -Up.y * Axes.y);
    float gl = length(g);
    float2 gd = g / max(gl, 0.0001);
    float2 perp = float2(-gd.y, gd.x);
    float grav = saturate((gl - 0.12) / 0.4);
    // Which start points hold a streak depends on the sweat alone, never on the surface's tilt: the tilt changes with every movement of the character, and
    // a streak that came and went with it would flicker. The tilt (grav) only fades a streak, smoothly; a rising sweat level fades new streaks in over a band.
    float run = smoothstep(0.20, 0.70, amount * zoneRun * 1.1) * step(0.01, Streak);
    float rh = max(min(lerp(0.10, 0.20, saturate(run)) * Size, 0.19), 0.02);
    float2 pS = uvc;
    float trailA = 0.0;
    float headA = 0.0;
    float2 nHead = float2(0.0, 0.0);
    [unroll]
    for (int k = 0; k < 6; k++)
    {
        float2 ck = floor(pS - gd * (0.7 * (float)k));
        float3 hC;
        MHS_HASH(ck + seedOff + 5.17, hC);
        float existsS = saturate((0.22 * run - hC.x) / 0.03) * grav;
        float2 sp = ck + 0.3 + 0.4 * float2(hC.y, hC.z);
        float runLen = min((1.6 + 2.0 * frac(hC.x * 7.13 + hC.y * 3.7)) * Streak, 3.6);
        float period = 5.0 + 9.0 * frac(hC.y * 11.1 + hC.z * 5.3);
        float cyc = frac(GameTime / period + hC.z);
        float headD = runLen * saturate(cyc / 0.8);
        float life = (1.0 - saturate((cyc - 0.8) / 0.2)) * saturate(cyc / 0.08) * existsS;
        float2 dS = pS - sp;
        float a = dot(dS, gd);
        float lat = dot(dS, perp);
        float along = saturate(a / max(headD, 0.05));
        float halfW = lerp(0.06, 0.15, along);
        float inTrail = step(0.0, a) * step(a, headD);
        float trail = inTrail * (1.0 - smoothstep(halfW * 0.4, halfW, abs(lat))) * pow(along, 1.2) * life;
        float2 qh = dS - gd * headD;
        float dh = length(qh) / rh;
        float headBead = smoothstep(0.0, 0.08, saturate(1.0 - dh * dh)) * life * beadFade;
        trailA = max(trailA, trail);
        headA = max(headA, headBead);
        nHead += qh / rh * headBead * 0.45;
    }
    // Trails are glossy bright lines (bead-level gloss and lift), so they stand out from the sheen around them.
    beadAlpha = max(beadAlpha, max(headA, trailA * 0.8));
    filmAlpha = max(filmAlpha, max(trailA * 0.85, headA * 0.6));
    nDelta += nHead;
}

// 3. Rain and immersion: fine droplets everywhere (never fully masked) over a thin damp film.
{
    float zoneW = 0.35 + 0.65 * zoneFilm;
    float2 pF = (uvc + warp) * 2.6;
    float2 cF = floor(pF);
    float2 fF = pF - cF;
    float3 hE;
    MHS_HASH(cF + seedOff + 71.3, hE);
    float growW = saturate(wet * zoneW * 1.2);
    float presentW = step(hE.x, 0.8 * pow(growW, 0.7)) * step(0.001, growW);
    float rW = min(lerp(0.08, 0.22, growW) * (0.5 + 0.9 * hE.y) * Size, 0.46);
    float2 cW = rW + (1.0 - 2.0 * rW) * float2(hE.z, frac(hE.x * 13.7 + hE.y * 7.3));
    float2 qW = fF - cW;
    float dW = length(qW) / rW;
    float aW = smoothstep(0.0, 0.08, saturate(1.0 - dW * dW)) * presentW * dropFade;
    beadAlpha = max(beadAlpha, aW);
    nDelta += qW / rW * aW * 0.35;
    filmAlpha = max(filmAlpha, max(aW * 0.5, (1.0 - dropFade) * growW * 0.3));
}

// The relief is built in (U, V) axes; Axes.xy (+-1) is the same orientation flip the downhill direction uses, so a wrong V sense of the tangent frame is one console value, not saddle-shaped beads.
nDelta = clamp(nDelta * Axes.xy * levelGate, float2(-1.2, -1.2), float2(1.2, 1.2));
return float4(saturate(beadAlpha * levelGate), saturate(max(sheen, filmAlpha * levelGate)), nDelta.x, nDelta.y);
