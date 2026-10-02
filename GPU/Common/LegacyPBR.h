#pragma once

#include <fstream>
#include <string>

#include "Common/GPU/ShaderWriter.h"
#include "Core/Config.h"
#include "Common/StringUtils.h"

inline bool LegacyPBRVertexMode1(bool doTexture, bool useHWTransform, bool hasNormal, bool hasTexcoord, bool enableFog, bool enableLighting, bool vertexRangeCulling) {
	return doTexture && useHWTransform && hasNormal && hasTexcoord && enableFog && enableLighting && vertexRangeCulling;
}

inline bool LegacyPBRVertexMode2(bool doTexture, bool useHWTransform, bool hasTexcoord, bool enableFog, bool enableLighting, bool vertexRangeCulling) {
	return doTexture && useHWTransform && hasTexcoord && enableFog && enableLighting && vertexRangeCulling;
}

inline bool LegacyPBRFragmentBase(bool enableFragmentTestCache, bool doTexture, bool enableFog) {
	return enableFragmentTestCache && doTexture && enableFog;
}

inline void WriteLegacyPBRVaryingVS(ShaderWriter &p, const char *varying) {
	p.F("%s lowp flat int flag;\n", varying);
	for (int i = 1; i <= 8; ++i)
		p.F("%s highp vec4 v_%d;\n", varying, i);
}

inline void WriteLegacyPBRVaryingFS(ShaderWriter &p, const char *shading, const char *varying) {
	p.F("precision highp float;\n");
	p.F("%s %s lowp flat int flag;\n", shading, varying);
	for (int i = 1; i <= 8; ++i)
		p.F("%s %s highp vec4 v_%d;\n", shading, varying, i);
}

inline void WriteLegacyPBRPrelude(ShaderWriter &p) {
	p.C("vec3 albedo;\n");
	p.C("float metallic = 0.5;\n");
	p.C("float roughness = 0.15;\n");
	p.C("vec3 lightPositions[2];\n");
	p.C("vec3 lightColors[2];\n");
	p.C("vec3 camPos = vec3(0.0, 0.0, 50.0);\n");
	p.C("const float PI = 3.14159265359;\n");
	p.C("float lightness(float R, float G, float B) { return pow(pow(R / 1.0, 2.2) + pow(G / 0.666666, 2.2) + pow(B / 1.666666, 2.2), 1.0 / 2.2) * 0.547373; }\n");
	p.C("float DistributionGGX(vec3 N, vec3 H, float r) { float a = r*r; float a2 = a*a; float n = max(dot(N,H),0.0); float n2=n*n; float nom=a2; float denom=(n2*(a2-1.0)+1.0); denom=PI*denom*denom; return nom/denom; }\n");
	p.C("float GeometrySchlickGGX(float NdotV, float r) { float k=(r+1.0)*(r+1.0)/8.0; return NdotV/(NdotV*(1.0-k)+k); }\n");
	p.C("float GeometrySmith(vec3 N, vec3 V, vec3 L, float r) { float a=max(dot(N,V),0.0); float b=max(dot(N,L),0.0); return GeometrySchlickGGX(a,r)*GeometrySchlickGGX(b,r); }\n");
	p.C("vec3 fresnelSchlick(float c, vec3 F0) { return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0); }\n");
	p.C("void PBR__2_0() {\n");
	p.C("  vec3 N=normalize(v_1.xyz);\n");
	p.C("  vec3 fogcolor=vec3(mix(vec3(dot(u_fogcolor,vec3(0.299,0.587,0.114))),u_fogcolor,0.5));\n");
	p.C("  float l=lightness(u_fogcolor.r,u_fogcolor.g,u_fogcolor.b);\n");
	p.C("  vec3 ld; float ll; float minv; fogcolor += 0.75-l; vec3 fogcolor_inverse=1.0-fogcolor; vec3 diff1,diff2;\n");
	p.C("  if(l<0.15){ minv=0.2; ll=1.5; ld=normalize(v_5.xyz); ld.z=abs(ld.z)*7.0; diff1=fogcolor*ll; diff2=fogcolor_inverse*ll; lightPositions[0].z=v_3.z*1000.0; lightPositions[1].z=v_4.z*1000.0; lightColors[0]=fogcolor_inverse; lightColors[1]=fogcolor; } else { minv=0.1; ll=2.0; ld=normalize(v_4.xyz); ld.z=abs(ld.z)*5.0; diff1=fogcolor_inverse; diff2=fogcolor*ll; lightPositions[0].z=v_4.z*1000.0; lightPositions[1].z=v_3.z*1000.0; lightColors[0]=fogcolor; lightColors[1]=fogcolor_inverse; }\n");
	p.C("  lightPositions[0].x=N.x*1000.0; lightPositions[0].y=N.y*2000.0; lightPositions[1].x=N.x*3500.0; lightPositions[1].y=N.y*5000.0;\n");
	p.C("  vec3 lightDir=normalize(ld*1000.0-v_2.xyz); float diff=max(dot(N,lightDir),0.0); if(diff<minv) diff=minv; vec3 diffColor=mix(diff1,diff2,diff);\n");
	p.C("  if(gl_FragCoord.w>0.015){ vec3 V=normalize(camPos-v_2.xyz); vec3 F0=vec3(0.04); F0=mix(F0,albedo,metallic); vec3 Lo=vec3(0.0); for(int i=0;i<2;++i){ vec3 L=normalize(lightPositions[i]-v_2.xyz); vec3 H=normalize(V+L); float NDF=DistributionGGX(N,H,roughness); float G=GeometrySmith(N,V,L,roughness); vec3 F=fresnelSchlick(clamp(dot(H,V),0.0,1.0),F0); vec3 numerator=NDF*G*F; float denominator=4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0)+0.0001; vec3 specular=numerator/denominator; vec3 kS=F; vec3 kD=(vec3(1.0)-kS)*(1.0-metallic); float NdotL=max(dot(N,L),0.0); Lo+=(kD*albedo/PI+specular)*lightColors[i]*NdotL; } vec3 color=Lo+albedo; color=color/(color+vec3(1.0)); color*=diffColor; fragColor0=vec4(color,1.0); } else { fragColor0=vec4(albedo*diffColor*0.75,1.0); }\n");
	p.C("}\n");
}

inline bool LoadLegacyGLSLOverride(const std::string &stage, uint64_t shaderID, std::string *source) {
	const std::string path = (g_Config.memStickDirectory / "PSP/SHADERS/GLSL").ToString() + "/" + stage + "_" + StringFromFormat("%016llx", (unsigned long long)shaderID) + ".glsl";
	std::ifstream f(path);
	if (!f.is_open())
		return false;
	*source = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	return !source->empty() && source->size() + 1 < 32768;
}
