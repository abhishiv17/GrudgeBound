#include "GbAuthSubsystem.h"
#include "GbSettings.h"
#include "GbJson.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <dpapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	FString SessionPath() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SignIn.dat")); }
	FString JsonText(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(Object, Writer);
		return Text;
	}
}

void UGbAuthSubsystem::Deinitialize()
{
	if (GetGameInstance()) GetGameInstance()->GetTimerManager().ClearTimer(DeviceTimer);
	Token.Reset(); DeviceCode.Reset();
	Super::Deinitialize();
}

void UGbAuthSubsystem::Request(const FString& Path, const FString& Body, FReply Reply, bool bGet)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(GetDefault<UGbSettings>()->GetServerUrl() + Path);
	R->SetVerb(bGet ? TEXT("GET") : TEXT("POST"));
	R->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!Token.IsEmpty()) R->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	if (!bGet) R->SetContentAsString(Body);
	R->SetTimeout(60.f); // Initial account creation can take several testnet transactions.
	R->OnProcessRequestComplete().BindWeakLambda(this, [Reply = MoveTemp(Reply)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		Reply(bConnected && Response.IsValid() ? Response->GetResponseCode() : 0, Response.IsValid() ? Response->GetContentAsString() : FString());
	});
	R->ProcessRequest();
}

void UGbAuthSubsystem::ResumeSession()
{
	if (bBusy || IsSignedIn()) return;
	bBusy = true; bResuming = true;
	Message = TEXT("Looking for your place in the ledger...");
#if PLATFORM_WINDOWS
	TArray<uint8> Encrypted;
	if (FFileHelper::LoadFileToArray(Encrypted, *SessionPath()) && !Encrypted.IsEmpty())
	{
		DATA_BLOB In = { static_cast<DWORD>(Encrypted.Num()), Encrypted.GetData() };
		DATA_BLOB Out = {};
		if (CryptUnprotectData(&In, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
		{
			FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR*>(Out.pbData), static_cast<int32>(Out.cbData));
			const FString Text(Decoded.Length(), Decoded.Get());
			LocalFree(Out.pbData);
			TSharedPtr<FJsonObject> O;
			if (GbJson::ParseObject(Text, O) && O->GetStringField(TEXT("server")) == GetDefault<UGbSettings>()->GetServerUrl()) Token = O->GetStringField(TEXT("token"));
		}
	}
#endif
	if (Token.IsEmpty()) { bBusy = false; bResuming = false; Message = TEXT("Sign in to enter Hollowmere."); OnSessionChanged.Broadcast(); return; }
	Request(TEXT("/auth/session"), FString(), [this](int32 Status, const FString& Text)
	{
		bBusy = false; bResuming = false;
		TSharedPtr<FJsonObject> O;
		if (Status == 200 && GbJson::ParseObject(Text, O)) AcceptSession(O);
		else if (Status == 401) SessionExpired();
		else { Message = TEXT("Hollowmere is unreachable. Try signing in again."); OnSessionChanged.Broadcast(); }
	}, true);
	OnSessionChanged.Broadcast();
}

void UGbAuthSubsystem::BeginSignIn()
{
	if (bBusy || IsSignedIn()) return;
	Token.Reset();
	bBusy = true; Message = TEXT("Opening the gate..."); OnSessionChanged.Broadcast();
	Request(TEXT("/auth/device/start"), TEXT("{}"), [this](int32 Status, const FString& Text)
	{
		TSharedPtr<FJsonObject> O;
		if (Status != 200 || !GbJson::ParseObject(Text, O))
		{
			bBusy = false; Message = Status == 0 ? TEXT("Hollowmere is unreachable. Try again.") : GbJson::ErrorMessage(Text); OnSessionChanged.Broadcast(); return;
		}
		DeviceCode = O->GetStringField(TEXT("deviceCode"));
		DeviceExpiresAt = FPlatformTime::Seconds() + O->GetNumberField(TEXT("expiresIn"));
		const FString BrowserUrl = O->GetStringField(TEXT("browserUrl"));
		// Never launch arbitrary URL schemes received over the network.
		if (!(BrowserUrl.StartsWith(TEXT("https://")) || BrowserUrl.StartsWith(TEXT("http://localhost:")) || BrowserUrl.StartsWith(TEXT("http://127.0.0.1:"))))
		{
			bBusy = false; DeviceCode.Reset(); Message = TEXT("The sign-in address is invalid."); OnSessionChanged.Broadcast(); return;
		}
		FPlatformProcess::LaunchURL(*BrowserUrl, nullptr, nullptr);
		Message = TEXT("Check your browser to sign in, then return here."); OnSessionChanged.Broadcast();
		GetGameInstance()->GetTimerManager().SetTimer(DeviceTimer, this, &UGbAuthSubsystem::PollDevice, 2.f, false);
	});
}

void UGbAuthSubsystem::PollDevice()
{
	if (DeviceCode.IsEmpty()) return;
	if (FPlatformTime::Seconds() >= DeviceExpiresAt)
	{
		DeviceCode.Reset(); bBusy = false; Message = TEXT("Sign-in timed out. Please try again."); OnSessionChanged.Broadcast(); return;
	}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("deviceCode"), DeviceCode);
	Request(TEXT("/auth/device/poll"), JsonText(Body), [this](int32 Status, const FString& Text)
	{
		if (DeviceCode.IsEmpty()) return;
		TSharedPtr<FJsonObject> O;
		if (Status == 200 && GbJson::ParseObject(Text, O) && !O->GetBoolField(TEXT("pending")))
		{
			DeviceCode.Reset(); bBusy = false; Token = O->GetStringField(TEXT("token")); AcceptSession(O); return;
		}
		if (Status != 0 && Status != 200 && Status != 429)
		{
			DeviceCode.Reset(); bBusy = false; Message = GbJson::ErrorMessage(Text); OnSessionChanged.Broadcast(); return;
		}
		GetGameInstance()->GetTimerManager().SetTimer(DeviceTimer, this, &UGbAuthSubsystem::PollDevice, 2.f, false);
	});
}

void UGbAuthSubsystem::AcceptSession(const TSharedPtr<FJsonObject>& Json)
{
	const TSharedPtr<FJsonObject> P = Json->GetObjectField(TEXT("player"));
	Account = P->GetStringField(TEXT("account")); PlayerName = P->GetStringField(TEXT("name"));
	Message = TEXT("Welcome to Hollowmere."); SaveSession(); OnSessionChanged.Broadcast();
}

void UGbAuthSubsystem::SaveSession()
{
#if PLATFORM_WINDOWS
	const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("token"), Token); O->SetStringField(TEXT("server"), GetDefault<UGbSettings>()->GetServerUrl());
	const FTCHARToUTF8 Utf8(*JsonText(O));
	DATA_BLOB In = { static_cast<DWORD>(Utf8.Length()), reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(Utf8.Get())) };
	DATA_BLOB Out = {};
	if (CryptProtectData(&In, L"Grudgebound session", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
	{
		TArray<uint8> Bytes; Bytes.Append(Out.pbData, static_cast<int32>(Out.cbData)); LocalFree(Out.pbData);
		IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(), true);
		FFileHelper::SaveArrayToFile(Bytes, *SessionPath());
	}
#endif
}

void UGbAuthSubsystem::ClearSession()
{
	Token.Reset(); Account.Reset(); PlayerName.Reset(); DeviceCode.Reset(); bBusy = false; bResuming = false;
	GetGameInstance()->GetTimerManager().ClearTimer(DeviceTimer);
	IFileManager::Get().Delete(*SessionPath(), false, true);
}

void UGbAuthSubsystem::SignOut()
{
	// Request copies the token into its headers before ClearSession forgets it locally.
	if (!Token.IsEmpty()) Request(TEXT("/auth/logout"), TEXT("{}"), [](int32, const FString&) {});
	ClearSession(); Message = TEXT("You have signed out."); OnSessionChanged.Broadcast();
}

void UGbAuthSubsystem::SessionExpired()
{
	ClearSession(); Message = TEXT("Your sign-in expired. Please sign in again."); OnSessionChanged.Broadcast();
}
