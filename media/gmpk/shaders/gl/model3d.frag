// 相机视角法向量
vec3 g_model3d_normal_eye;
vec3 g_model3d_ambientLight;
vec3 g_model3d_diffuseLight;
vec3 g_model3d_specularLight;
vec3 g_model3d_refractionLight;

in vec4 _model3d_position_world;

vec4 normalToTexture(vec3 normal_N)
{
    // 先反转z，按照大家的习惯来展现法线颜色
    normal_N = vec3(normal_N.xy, -normal_N.z);
    return vec4( (normal_N + 1) * .5f, 1);
}

void GM_Model3D()
{
    // 如果是Replace，直接使用此颜色作为目标颜色
    if (GM_ColorVertexOp == GM_VertexColorOp_Replace)
    {
        _frag_color = _color;
        return;
    }

    PS_3D_INPUT vertex;
    vertex.WorldPos = _model3d_position_world.xyz;

    mat3 inverse_transpose_model_matrix = mat3(GM_InverseTransposeModelMatrix);
    vertex.Normal_World_N = normalize(inverse_transpose_model_matrix * _normal.xyz);

    mat3 normalEyeTransform = mat3(GM_ViewMatrix) * inverse_transpose_model_matrix;
    vec3 normal_World_N = normalEyeTransform * _normal.xyz;
    vertex.Normal_Eye_N = normalize( normal_World_N );

    GMTangentSpace tangentSpace;
    if (GM_IsTangentSpaceInvalid(_tangent.xyz, _bitangent.xyz))
    {
        tangentSpace = GM_CalculateTangentSpaceRuntime(vertex.WorldPos, _uv, _normal.xyz, GM_NormalMapTextureAttribute);
    }
    else
    {
        vec3 tangent_eye = normalize(normalEyeTransform * _tangent.xyz);
        vec3 bitangent_eye = normalize(normalEyeTransform * _bitangent.xyz);
        mat3 TBN = transpose(mat3(
            tangent_eye,
            bitangent_eye,
            vertex.Normal_Eye_N
        ));
        tangentSpace.TBN = TBN;
        tangentSpace.Normal_Tangent_N = GM_SampleTextures(GM_NormalMapTextureAttribute, _uv).rgb * 2.0 - 1.0;
    }

    vertex.HasNormalMap = GM_NormalMapTextureAttribute.Enabled != 0;

    /// Start Debug Option
    if (GM_Debug_Normal == GM_Debug_Normal_WorldSpace)
    {
        if (vertex.HasNormalMap)
            _frag_color = normalToTexture(normalize(mat3(GM_InverseViewMatrix) * transpose(tangentSpace.TBN) * tangentSpace.Normal_Tangent_N));
        else
            _frag_color = normalToTexture(vertex.Normal_World_N.xyz);
        return;
    }
    else if (GM_Debug_Normal == GM_Debug_Normal_EyeSpace)
    {
        if (vertex.HasNormalMap)
            _frag_color = normalToTexture(normalize(transpose(tangentSpace.TBN) * tangentSpace.Normal_Tangent_N));
        else
            _frag_color = normalToTexture(vertex.Normal_Eye_N.xyz);
        return;
    }
    /// End Debug Option

    vertex.TangentSpace = tangentSpace;
    vertex.IlluminationModel = GM_IlluminationModel;
    if (GM_IlluminationModel == GM_IlluminationModel_Phong)
    {
        vertex.Shininess = GM_Material.Shininess;
        vertex.Refractivity = GM_Material.Refractivity;
        vertex.AmbientLightmapTexture = GM_SampleTextures(GM_AmbientTextureAttribute, _uv).rgb
             * GM_SampleTextures(GM_LightmapTextureAttribute, _lightmapuv).rgb * GM_Material.Ka;
        vertex.DiffuseTexture = GM_SampleTextures(GM_DiffuseTextureAttribute, _uv).rgb * GM_Material.Kd;
        vertex.SpecularTexture = GM_SampleTextures(GM_SpecularTextureAttribute, _uv).rrr * GM_Material.Ks;
    }
    else if (GM_IlluminationModel == GM_IlluminationModel_CookTorranceBRDF)
    {
        vertex.AlbedoTexture = pow(GM_SampleTextures(GM_AlbedoTextureAttribute, _uv).rgb, vec3(GM_Gamma));
        vertex.MetallicRoughnessAOTexture = GM_SampleTextures(GM_MetallicRoughnessAOTextureAttribute, _uv).rgb;
        vertex.F0 = GM_Material.F0;
    }
    _frag_color = PS_3D_CalculateColor(vertex);

    if (GM_ColorVertexOp == GM_VertexColorOp_Multiply)
        _frag_color *= _color;
    else if (GM_ColorVertexOp == GM_VertexColorOp_Add)
        _frag_color += _color;
    // else (GM_ColorVertexOp == 0) do nothing
}
