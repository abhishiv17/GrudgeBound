// Greybox helpers: engine basic shapes, tinted with the paper-and-ink palette (docs/ART_PROMPTS.md).
// Only for the Phase 1 greybox; the art pass replaces these meshes.
#pragma once

#include "CoreMinimal.h"

class UInstancedStaticMeshComponent;
class UObject;

namespace GbGreybox
{
	enum class EShape : uint8 { Cube, Cone, Cylinder, Sphere };

	/** Call from a constructor only: sets the basic-shape mesh and material (hard references, so they get cooked). */
	void SetupMesh(UInstancedStaticMeshComponent* Mesh, EShape Shape);

	/** Tints the mesh with a dynamic instance of the basic-shape material. */
	void Paint(UInstancedStaticMeshComponent* Mesh, const FLinearColor& Color, UObject* Outer);

	/** Scatters grass blades (cones) over a SizeMetres patch centred on the component. */
	void FillGrassBlades(UInstancedStaticMeshComponent* Mesh, FIntPoint SizeMetres, int32 PerMetre, int32 Seed);

	namespace Color
	{
		inline FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

		inline const FLinearColor Ink = Hex(TEXT("1A1726"));
		inline const FLinearColor Paper = Hex(TEXT("F3E9D2"));
		inline const FLinearColor Gold = Hex(TEXT("D9A532"));
		inline const FLinearColor Crimson = Hex(TEXT("B0153A"));
		inline const FLinearColor Rage = Hex(TEXT("C8402A"));
		inline const FLinearColor Sorrow = Hex(TEXT("2B6CB0"));
		inline const FLinearColor Dread = Hex(TEXT("6B46C1"));
		inline const FLinearColor Lantern = Hex(TEXT("F7C35C"));
		inline const FLinearColor Grass = Hex(TEXT("7C9A4C"));
		inline const FLinearColor TallGrass = Hex(TEXT("507B36"));
		inline const FLinearColor Stone = Hex(TEXT("E6D6B2"));
		inline const FLinearColor StoneDark = Hex(TEXT("B09A70"));
		inline const FLinearColor Mortar = Hex(TEXT("A48C63"));
		inline const FLinearColor Water = Hex(TEXT("2E97A3"));
		inline const FLinearColor Wood = Hex(TEXT("7A4E2E"));
		inline const FLinearColor WoodDark = Hex(TEXT("4A2E1E"));
		inline const FLinearColor Teal = Hex(TEXT("3A7CA5"));
		inline const FLinearColor Leaf = Hex(TEXT("4F8A3C"));
	}
}
