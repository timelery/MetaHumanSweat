// MetaHuman Sweat: normal blend. Inputs: float3 N, float4 Field. Output float3.
float3 n = float3(N.xy + Field.zw, N.z);
return normalize(n);
