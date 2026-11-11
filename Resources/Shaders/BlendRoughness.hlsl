// MetaHuman Sweat: roughness blend. Inputs: float Rough, float4 Field. Output float.
// Dry skin is MATTE (a chalkier, broader highlight than the stock skin's moist sheen); sweat is what brings the gloss back, far past the stock value.
float f = saturate(Field.y);
float matte = saturate(Rough * 1.15 + 0.10);
// A film of water has its own gloss: three quarters of the wet value is absolute, so the face and body skin materials (different roughness textures)
// meet at nearly the same gloss when wet instead of keeping their dry difference across the neck.
float r = lerp(matte, lerp(Rough * 0.40, 0.20, 0.75), smoothstep(0.0, 0.85, f));
r = lerp(r, min(r, 0.12), saturate(Field.x));
return saturate(r);
