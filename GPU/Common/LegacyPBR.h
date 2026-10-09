#pragma once

#include <string>

#include "Common/GPU/ShaderWriter.h"
#include "Core/Config.h"
#include "Core/Util/PathUtil.h"
#include "Common/File/FileUtil.h"
#include "Common/StringUtils.h"

// The legacy vertex flags included _doTexture and _enableFog, but the modern
// VShaderID does not carry either fragment-state bit. Those conditions are
// validated by the fragment shader when it selects the legacy PBR path.
// Here we only test state represented by VShaderID.
inline bool LegacyPBRVertexMode1(bool useHWTransform, bool hasNormal, bool hasTexcoord, bool enableLighting, bool vertexRangeCulling) {
	return useHWTransform && hasNormal && hasTexcoord && enableLighting && vertexRangeCulling;
}

inline bool LegacyPBRVertexMode2(bool useHWTransform, bool hasTexcoord, bool enableLighting, bool vertexRangeCulling) {
	return useHWTransform && hasTexcoord && enableLighting && vertexRangeCulling;
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

	p.C("float lightness(float R, float G, float B) {\n");
	p.C("    return pow(\n");
	p.C("        pow(R / 1.0, 2.2) +\n");
	p.C("        pow(G / 0.666666, 2.2) +\n");
	p.C("        pow(B / 1.666666, 2.2),\n");
	p.C("        1.0 / 2.2\n");
	p.C("    ) * 0.547373;\n");
	p.C("}\n");

	p.C("float DistributionGGX(vec3 N, vec3 H, float r) {\n");
	p.C("    float a = r * r;\n");
	p.C("    float a2 = a * a;\n");
	p.C("    float NdotH = max(dot(N, H), 0.0);\n");
	p.C("    float NdotH2 = NdotH * NdotH;\n");
	p.C("    float numerator = a2;\n");
	p.C("    float denominator = NdotH2 * (a2 - 1.0) + 1.0;\n");
	p.C("    denominator = PI * denominator * denominator;\n");
	p.C("    return numerator / denominator;\n");
	p.C("}\n");

	p.C("float GeometrySchlickGGX(float NdotV, float r) {\n");
	p.C("    float k = (r + 1.0) * (r + 1.0) / 8.0;\n");
	p.C("    return NdotV / (NdotV * (1.0 - k) + k);\n");
	p.C("}\n");

	p.C("float GeometrySmith(vec3 N, vec3 V, vec3 L, float r) {\n");
	p.C("    float NdotV = max(dot(N, V), 0.0);\n");
	p.C("    float NdotL = max(dot(N, L), 0.0);\n");
	p.C("    float ggxV = GeometrySchlickGGX(NdotV, r);\n");
	p.C("    float ggxL = GeometrySchlickGGX(NdotL, r);\n");
	p.C("    return ggxV * ggxL;\n");
	p.C("}\n");

	p.C("vec3 fresnelSchlick(float cosTheta, vec3 F0) {\n");
	p.C("    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);\n");
	p.C("}\n");

	p.C("void PBR__2_0() {\n");
	p.C("    vec3 N = normalize(v_1.xyz);\n");
	p.C("    vec3 fogcolor = vec3(mix(\n");
	p.C("        vec3(dot(u_fogcolor, vec3(0.299, 0.587, 0.114))),\n");
	p.C("        u_fogcolor,\n");
	p.C("        0.5\n");
	p.C("    ));\n");
	p.C("    float l = lightness(u_fogcolor.r, u_fogcolor.g, u_fogcolor.b);\n");
	p.C("    vec3 ld;\n");
	p.C("    float ll;\n");
	p.C("    float minv;\n");
	p.C("    fogcolor += 0.75 - l;\n");
	p.C("    vec3 fogcolor_inverse = 1.0 - fogcolor;\n");
	p.C("    vec3 diff1;\n");
	p.C("    vec3 diff2;\n");

	p.C("    if (l < 0.15) {\n");
	p.C("        minv = 0.2;\n");
	p.C("        ll = 1.5;\n");
	p.C("        ld = normalize(v_5.xyz);\n");
	p.C("        ld.z = abs(ld.z) * 7.0;\n");
	p.C("        diff1 = fogcolor * ll;\n");
	p.C("        diff2 = fogcolor_inverse * ll;\n");
	p.C("        lightPositions[0].z = v_3.z * 1000.0;\n");
	p.C("        lightPositions[1].z = v_4.z * 1000.0;\n");
	p.C("        lightColors[0] = fogcolor_inverse;\n");
	p.C("        lightColors[1] = fogcolor;\n");
	p.C("    } else {\n");
	p.C("        minv = 0.1;\n");
	p.C("        ll = 2.0;\n");
	p.C("        ld = normalize(v_4.xyz);\n");
	p.C("        ld.z = abs(ld.z) * 5.0;\n");
	p.C("        diff1 = fogcolor_inverse;\n");
	p.C("        diff2 = fogcolor * ll;\n");
	p.C("        lightPositions[0].z = v_4.z * 1000.0;\n");
	p.C("        lightPositions[1].z = v_3.z * 1000.0;\n");
	p.C("        lightColors[0] = fogcolor;\n");
	p.C("        lightColors[1] = fogcolor_inverse;\n");
	p.C("    }\n");

	p.C("    lightPositions[0].x = N.x * 1000.0;\n");
	p.C("    lightPositions[0].y = N.y * 2000.0;\n");
	p.C("    lightPositions[1].x = N.x * 3500.0;\n");
	p.C("    lightPositions[1].y = N.y * 5000.0;\n");

	p.C("    vec3 lightDir = normalize(ld * 1000.0 - v_2.xyz);\n");
	p.C("    float diff = max(dot(N, lightDir), 0.0);\n");
	p.C("    if (diff < minv) {\n");
	p.C("        diff = minv;\n");
	p.C("    }\n");
	p.C("    vec3 diffColor = mix(diff1, diff2, diff);\n");

	p.C("    if (gl_FragCoord.w > 0.015) {\n");
	p.C("        vec3 V = normalize(camPos - v_2.xyz);\n");
	p.C("        vec3 F0 = vec3(0.04);\n");
	p.C("        F0 = mix(F0, albedo, metallic);\n");
	p.C("        vec3 Lo = vec3(0.0);\n");
	p.C("        for (int i = 0; i < 2; ++i) {\n");
	p.C("            vec3 L = normalize(lightPositions[i] - v_2.xyz);\n");
	p.C("            vec3 H = normalize(V + L);\n");
	p.C("            float NDF = DistributionGGX(N, H, roughness);\n");
	p.C("            float G = GeometrySmith(N, V, L, roughness);\n");
	p.C("            vec3 F = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);\n");
	p.C("            vec3 numerator = NDF * G * F;\n");
	p.C("            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;\n");
	p.C("            vec3 specular = numerator / denominator;\n");
	p.C("            vec3 kS = F;\n");
	p.C("            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);\n");
	p.C("            float NdotL = max(dot(N, L), 0.0);\n");
	p.C("            Lo += (kD * albedo / PI + specular) * lightColors[i] * NdotL;\n");
	p.C("        }\n");
	p.C("        vec3 color = Lo + albedo;\n");
	p.C("        color = color / (color + vec3(1.0));\n");
	p.C("        color *= diffColor;\n");
	p.C("        fragColor0 = vec4(color, 1.0);\n");
	p.C("    } else {\n");
	p.C("        fragColor0 = vec4(albedo * diffColor * 0.75, 1.0);\n");
	p.C("    }\n");
	p.C("}\n");
}

inline Path LegacyGLSLOverridePath(const std::string &stage, uint64_t shaderID) {
	// memStickDirectory may itself be the PSP directory on Android (e.g. /sdcard/PSP).
	// GetSysDirectory(DIRECTORY_PSP) normalizes both cases:
	//   /sdcard/PSP     -> /sdcard/PSP
	//   /sdcard/PPSSPP  -> /sdcard/PPSSPP/PSP
	const Path shaderDirectory = GetSysDirectory(DIRECTORY_PSP) / "SHADERS/GLSL";
	return shaderDirectory / (stage + "_" + StringFromFormat("%016llx", (unsigned long long)shaderID) + ".glsl");
}

inline bool LoadLegacyGLSLOverride(const std::string &stage, uint64_t shaderID, std::string *source) {
	const Path path = LegacyGLSLOverridePath(stage, shaderID);
	return File::ReadTextFileToString(path, source) && !source->empty() && source->size() + 1 < 32768;
}

inline bool SaveGeneratedLegacyGLSL(const std::string &stage, uint64_t shaderID, std::string_view source) {
	const Path path = LegacyGLSLOverridePath(stage, shaderID);

	// Never overwrite a user-provided shader. If it already exists, the caller
	// will load and use it instead.
	if (File::Exists(path))
		return false;

	if (!File::CreateFullPath(Path(path.GetDirectory())))
		return false;

	return File::WriteStringToFile(true, source, path);
}
