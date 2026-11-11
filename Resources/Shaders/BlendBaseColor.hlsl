// MetaHuman Sweat: base colour blend. Inputs: float3 Base, float4 Field. Output float3.
float3 c = Base * lerp(float3(1.0, 1.0, 1.0), float3(0.90, 0.88, 0.88), saturate(Field.y));
// A bead is a lens over the skin: lighter than the skin around it (it catches the room), not a dark spot.
c = lerp(c, c * 1.25 + float3(0.03, 0.027, 0.022), saturate(Field.x));
return c;
