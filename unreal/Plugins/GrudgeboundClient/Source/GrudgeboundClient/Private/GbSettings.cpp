#include "GbSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UGbSettings::UGbSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Grudgebound");
}

FString UGbSettings::GetServerUrl() const
{
	FString Url = ServerUrl;
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("GbServer="), Override) && !Override.IsEmpty())
	{
		Url = Override;
	}
	Url.TrimStartAndEndInline();
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
	}
	return Url;
}
