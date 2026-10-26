/**
 * NFS_MW_Opcoes - painel de opcoes do mod de vibracao do NFS Most Wanted.
 * Nao altera a imagem: serve so para mostrar estas opcoes no menu do ReShade (tecla Home).
 * O programa mw_rumble le os valores salvos no preset do ReShade.
 * Sem acentos de proposito: a fonte do menu nao mostra acentos.
 */

uniform bool Vibracao_Ligada <
	ui_category = "Vibracao do controle";
	ui_label = "Vibracao ligada";
	ui_tooltip = "Liga/desliga toda a vibracao. Precisa do mw_rumble aberto.";
> = true;

uniform float Vibracao_Intensidade <
	ui_type = "slider";
	ui_category = "Vibracao do controle";
	ui_label = "Intensidade geral";
	ui_min = 0.2; ui_max = 1.5; ui_step = 0.05;
	ui_tooltip = "0.85 e o padrao. Aumente para vibrar mais forte, diminua para mais fraco.";
> = 0.85;

uniform bool Vibracao_Batidas <
	ui_category = "Vibracao do controle";
	ui_label = "Batidas";
	ui_tooltip = "Vibra forte quando voce bate.";
> = true;

uniform bool Vibracao_Nitro <
	ui_category = "Vibracao do controle";
	ui_label = "Nitro";
	ui_tooltip = "Vibra enquanto o nitro esta ligado.";
> = true;

uniform bool Vibracao_Estrada <
	ui_category = "Vibracao do controle";
	ui_label = "Velocidade (vibracao leve)";
	ui_tooltip = "Vibracao suave que aumenta com a velocidade.";
> = true;

uniform bool Vibracao_Derrapagem <
	ui_category = "Vibracao do controle";
	ui_label = "Derrapagem";
	ui_tooltip = "Vibra ao derrapar.";
> = true;

uniform bool Vibracao_Pouso <
	ui_category = "Vibracao do controle";
	ui_label = "Pouso de saltos";
	ui_tooltip = "Vibra ao aterrissar depois de um salto.";
> = true;
