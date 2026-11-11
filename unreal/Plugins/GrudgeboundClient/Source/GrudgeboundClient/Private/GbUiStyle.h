// Shared look for every screen: the web client's pixel-paper style (client/app/styles/ui.css) in Slate.
// Notched ink frames, cream paper panels with crimson corner studs, bevelled buttons, Pixelify Sans for
// headings and buttons, Courier Prime for body text. Implemented in GbWidgets.cpp.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

namespace GbUi
{
	/** bBold: the pixel UI font (buttons, labels). bTitle: its heavy cut (headings). Neither: body text. */
	FSlateFontInfo Font(int32 Size, bool bBold = false, bool bTitle = false);

	/** Notched pixel frame around a flat face. bStuds adds the crimson corner studs of the big paper panels. */
	FSlateBrush PanelBrush(const FLinearColor& Tint, bool bStuds = false);
	/** Framed button face with a light top edge and a dark bottom edge. */
	FSlateBrush ButtonBrush(const FLinearColor& Tint);

	/** "riot-bell" -> "curse_riot_bell": the curse's picture in UI/Battle. */
	FString CurseArt(const FString& SpeciesId);

	inline FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	inline FLinearColor PaperHi() { return Hex(TEXT("FBF5E6")); }
	inline FLinearColor Paper2() { return Hex(TEXT("E9DBBB")); }
	inline FLinearColor Paper3() { return Hex(TEXT("D6C29A")); }
	inline FLinearColor InkSoft() { return Hex(TEXT("6B6276")); }
	inline FLinearColor Night() { return Hex(TEXT("0E0B26")); }
}
