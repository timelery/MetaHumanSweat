// MetaHuman Sweat: specular blend. Inputs: float Spec, float4 Field. Output float.
float s = lerp(Spec * 0.7, Spec, smoothstep(0.0, 0.85, saturate(Field.y)));
return lerp(s, max(Spec, 0.5), saturate(Field.x));
