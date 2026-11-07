// Browser email sign-in. The device secret stays in the game; only the approval link opens in the browser.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GbAuthSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGbSessionChanged);

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|SignIn") void ResumeSession();
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|SignIn") void BeginSignIn();
	UFUNCTION(BlueprintCallable, Category = "Grudgebound|SignIn") void SignOut();
	UFUNCTION(BlueprintPure, Category = "Grudgebound|SignIn") bool IsSignedIn() const { return !Account.IsEmpty() && !Token.IsEmpty(); }
	UFUNCTION(BlueprintPure, Category = "Grudgebound|SignIn") bool IsBusy() const { return bBusy; }
	UFUNCTION(BlueprintPure, Category = "Grudgebound|SignIn") FString GetMessage() const { return Message; }
	FString GetAccount() const { return Account; }
	FString GetPlayerName() const { return PlayerName; }
	FString GetToken() const { return Token; }
	void SessionExpired();
	UPROPERTY(BlueprintAssignable, Category = "Grudgebound|SignIn") FGbSessionChanged OnSessionChanged;
private:
	using FReply = TFunction<void(int32, const FString&)>;
	void Request(const FString& Path, const FString& Body, FReply Reply, bool bGet = false);
	void PollDevice();
	void AcceptSession(const TSharedPtr<class FJsonObject>& Json);
	void SaveSession();
	void ClearSession();
	FString Token;
	FString Account;
	FString PlayerName;
	FString DeviceCode;
	FString Message = TEXT("Sign in to enter Hollowmere.");
	bool bBusy = false;
	bool bResuming = false;
	double DeviceExpiresAt = 0;
	FTimerHandle DeviceTimer;
};
