
texture BackBufferTex : COLOR;
sampler BackBuffer { Texture = BackBufferTex; };

void VS_Tela(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
	texcoord.x = (id == 2) ? 2.0 : 0.0;
	texcoord.y = (id == 1) ? 2.0 : 0.0;
	position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 PS_Opcoes(float4 vpos : SV_Position, float2 tex : TEXCOORD) : SV_Target
{
	return tex2D(BackBuffer, tex);
}

technique NFS_MW_Opcoes <
	ui_tooltip = "Opcoes da vibracao e dos ajustes do NFS Most Wanted. Nao altera a imagem.";
>
{
	pass
	{
		VertexShader = VS_Tela;
		PixelShader = PS_Opcoes;
	}
}
