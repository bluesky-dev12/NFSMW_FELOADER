#include <string>
#include <D3dx9math.h>
#include <vector>

#include "IniReader/IniReader.h"
#include "Injector/injector.hpp"
#include "Injector/utility.hpp"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

struct Position
{
	const char* IniSection;
	const char* Name;
	int Preset;

	float X;
	float Y;
	float Z;
	float CarZ;
	int Rotation;
	bool Reflection;
	bool CustomPlatform;
};

struct TrackPositionMarker
{
	TrackPositionMarker* Prev;
	TrackPositionMarker* Next;
	int Hash;
	int blank;
	float X;
	float Y;
	float Z;
	float W;
	unsigned short Rotation;
};

const char* VERSION = "NFSMW - FE Loader 1.0";

const float DefaultCarZ = 0.275f;
const float DefaultShadowZ = 0;

Position Positions[6];
D3DXVECTOR4 CustomPlatformPosition;
std::string CustomPlatformPath;
bool LoadCustomPlatform = false;

void ToggleReflections(bool enabled)
{
	//injector::WriteMemory<BYTE>(0x0072E603, enabled ? 0x74 : 0xEB);
}

namespace Game
{
	float* ShadowZ = (float*)0x744232;
	int* GameState = (int*)0x925E90;

	float* CarZ = (float*)0x8916CC;
	float* DeltaTime = (float*)0x9259BC;
	float* FrontSteerAngle = (float*)0x905E20;

	char const* nullString = (char const*)0x890978;

	auto GetTrackPositionMarker = (TrackPositionMarker * (__cdecl*)(const char* a1, int a2))0x727340;
	auto StringHash = (int(__cdecl*)(const char*))0x460BF0;
	auto eViewPlatInterface_Render = (int(__thiscall*)(void*, void*, D3DXMATRIX*, D3DXMATRIX*, int, int))0x6DA9B0;
	auto eModel_Init = (void(__thiscall*)(void*, int))0x506AF0;
	auto eModel_GetBoundingBox = (void(__thiscall*)(void*, D3DXVECTOR3*, D3DXVECTOR3*))0x501570;
	auto LoadResourceFile = (void(__cdecl*)(const char* path, int type, int, int, int, int, int))0x661760;
	auto FindResourceFile = (int* (__cdecl*)(const char* path))0x659020;
	auto RenderWorld = (int(__thiscall*)(void*, void*, void*))0x723FA0;
	auto FeCustomizeParts_Ctor = (void(__thiscall*)(void*, void*))0x7C00C0;
	auto FeCustomizeParts_Dtor = (void(__thiscall*)(void*))0x7BCFF0;
	auto FeManager_Get = (DWORD*(*)())0x516E10;
	auto FeManager_GetGarageType = (int(__thiscall*)(DWORD*))0x516E20;
}

void InitGamePositions()
{
	Positions[0].IniSection = "POS_MAIN";
	Positions[0].Name = "";

	Positions[1].IniSection = "POS_PLATFORMCRIB";
	Positions[1].Name = "";

	Positions[2].IniSection = "POS_CAREER_SAFEHOUSE";
	Positions[2].Name = "";

	Positions[3].IniSection = "POS_CUSTOMIZATION_SHOP";
	Positions[3].Name = "";

	Positions[4].IniSection = "POS_CUSTOMIZATION_SHOP_BACKROOM";
	Positions[4].Name = "";

	Positions[5].IniSection = "POS_CAR_LOT";
	Positions[5].Name = "";
}

int GetGarageType()
{
	DWORD* TheFEManager = Game::FeManager_Get();

	if (TheFEManager)
	{
		return Game::FeManager_GetGarageType(TheFEManager);
	}

	return 0;
}

bool DrawCustomPlatform()
{
	return Positions[GetGarageType()].CustomPlatform;
}

std::vector<void*> GarageParts;
void* GarageScrollPart = NULL;
bool GarageInit = false;
D3DXMATRIX m;

struct PartEntry
{
	int hash;
	int* ptr;
};

bool StartsWith(char* str, const char* prefix)
{
	int slen = strlen(str);
	int plen = strlen(prefix);
	if (slen < plen)
	{
		return false;
	}

	for (int i = 0; i < plen; i++)
	{
		if (str[i] != prefix[i])
		{
			return false;
		}
	}

	return true;
}

float ScrollOffset = 0;
float ScrollLen = 0;
float ScrollAngle = 0;
int ScrollItems;
D3DXMATRIX* ScrollMatrises;
float ScrollSpeed, TargetScrollSpeed, ScrollSpeedMin, ScrollSpeedMax;
bool InitCustomGarage()
{

	if (!GarageInit)
	{
		int* recource = Game::FindResourceFile(CustomPlatformPath.c_str());
		if (recource)
		{
			D3DXMatrixScaling(&m, 1, 1, 1);
			m._41 = CustomPlatformPosition.x;
			m._42 = CustomPlatformPosition.y;
			m._43 = CustomPlatformPosition.z;
			m._44 = 1;

			int* geometry = (int*)recource[0xF];
			int src = 0;
			while (geometry[src] != 0x134003) src++;

			int size = geometry[src + 1] / 8;
			auto parts = (PartEntry*)(geometry + src + 2);

			for (int i = 0; i < size; i++)
			{
				auto part = parts[i];
				char* name = (char*)part.ptr + 0xA0;

				int* model = new int[6];
				Game::eModel_Init(model, Game::StringHash(name));
				if (StartsWith(name, "SCROLL_"))
				{
					GarageScrollPart = model;
					D3DXVECTOR3 a, b;
					Game::eModel_GetBoundingBox(GarageScrollPart, &a, &b);
					ScrollLen = abs(a.x) + abs(b.x) - 0.02f;
					ScrollMatrises = new D3DXMATRIX[ScrollItems * 2];
				}
				else
				{
					GarageParts.push_back(model);
				}
			}

			GarageInit = true;
		}
	}

	return GarageInit;
}

void Render(void* plat, void* model, D3DXMATRIX& matrix)
{
	Game::eViewPlatInterface_Render(plat, model, &matrix, 0, 0, 0);
}

void InitScrollMatrices()
{
	for (int i = 0; i < ScrollItems; i++)
	{
		ScrollMatrises[i] = m;
		ScrollMatrises[i]._41 += ScrollLen * i;
	}

	for (int i = ScrollItems, j = 1; i < ScrollItems * 2; i++, j++)
	{
		ScrollMatrises[i] = m;
		ScrollMatrises[i]._41 += -ScrollLen * j;
	}
}

void __stdcall DrawGarage(void* plat)
{
	if (InitCustomGarage())
	{
		for (auto model : GarageParts)
		{
			Render(plat, model, m);
		}

		if (GarageScrollPart)
		{
			for (int i = 0; i < ScrollItems * 2; i++)
			{
				Render(plat, GarageScrollPart, ScrollMatrises[i]);
			}
		}
	}
}

void MoveTowards(float& a, float b, float step)
{
	if (a < b)
	{
		a += step;
		if (a > b)
		{
			a = b;
		}
	}

	if (a > b)
	{
		a -= step;
		if (a < b)
		{
			a = b;
		}
	}
}

void Update()
{
	if (GarageScrollPart)
	{
		ScrollAngle += *Game::DeltaTime * ScrollSpeed * 1.5;
		if (ScrollAngle > 6.28f)
		{
			ScrollAngle -= 6.28f;
		}

		ScrollOffset += *Game::DeltaTime * ScrollSpeed;
		if (ScrollOffset > ScrollLen)
		{
			ScrollOffset -= ScrollLen;
		}

		InitScrollMatrices();

		for (int i = 0; i < ScrollItems * 2; i++)
		{
			ScrollMatrises[i]._41 -= ScrollOffset;
		}

		MoveTowards(ScrollSpeed, TargetScrollSpeed, *Game::DeltaTime * 5);
	}
}

void _fastcall DrawGarageMain(void* a1, int, void* a2, void* a3)
{
	if (*Game::GameState == 3 && DrawCustomPlatform())
	{
		Update();
		DrawGarage(a2);
	}

	Game::RenderWorld(a1, a2, a3);
}

void _fastcall DrawGarageReflection(void* a1, int, void* a2, void* a3)
{
	if (*Game::GameState == 3 && DrawCustomPlatform())
	{
		DrawGarage(a2);
	}

	Game::RenderWorld(a1, a2, a3);
}

void __stdcall GarageLoad()
{
	Game::LoadResourceFile(CustomPlatformPath.c_str(), 6, 0, 0, 0, 0, 0);
}

void __declspec(naked) GarageLoadCave()
{
	__asm
	{
		pushad;
		call GarageLoad;
		popad;

		ret;
	}
}

char const* __fastcall FEManager_GetGarageNameFromType(DWORD* FEManager, void* _edx)
{
	char const* result;

	int GarageType = GetGarageType();

	if (Positions[GarageType].CustomPlatform)
	{
		return CustomPlatformPath.c_str();
	}

	switch (GarageType)
	{
		case 1:
			result = "FRONTEND\\PLATFORMS\\PLATFORMCRIB.BIN";
			break;
		case 2:
			result = "FRONTEND\\PLATFORMS\\CAREER_SAFEHOUSE.BIN";
			break;
		case 3:
			result = "FRONTEND\\PLATFORMS\\CUSTOMIZATION_SHOP.BIN";
			break;
		case 4:
			result = "FRONTEND\\PLATFORMS\\CUSTOMIZATION_SHOP_BACKROOM.BIN";
			break;
		case 5:
			result = "FRONTEND\\PLATFORMS\\CAR_LOT.BIN";
			break;
		default:
			result = Game::nullString;
			break;
	}

	return result;
}

float GarageMainScreen_GetGeometryXPos() {return Positions[GetGarageType()].X;}

float GarageMainScreen_GetGeometryYPos() {return Positions[GetGarageType()].Y;}

float GarageMainScreen_GetGeometryZPos() {return Positions[GetGarageType()].Z;}

float GarageMainScreen_GetCarRotationZ() {return Positions[GetGarageType()].CarZ;}

bool IsFileExist(std::string path)
{
	char buffer[MAX_PATH];
	HMODULE hm = NULL;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, &hm);
	GetModuleFileNameA(hm, buffer, sizeof(buffer));

	std::string modulePath = buffer;
	path = modulePath.substr(0, modulePath.rfind('\\') + 1) + path;

	std::ifstream f(path.c_str());
	bool isGood = f.good();
	f.close();

	return isGood;
}

void __stdcall RotateFEWheels(int num, D3DXMATRIX* wheel, D3DXMATRIX* brake)
{
	if (GarageScrollPart && DrawCustomPlatform())
	{
		D3DXMATRIX rot;
		D3DXMatrixRotationY(&rot, ScrollAngle);
		D3DXMatrixMultiply(wheel, wheel, &rot);
	}
	else if (num <= 0x40)
	{
		D3DXMATRIX rot;
		D3DXMatrixRotationZ(&rot, *Game::FrontSteerAngle * 3.14 / 180.0);
		D3DXMatrixMultiply(wheel, wheel, &rot);
		D3DXMatrixMultiply(brake, brake, &rot);
	}
}

void __declspec(naked) UpdateRenderingCarParametersCave()
{
	static constexpr auto cExit = 0x7A9C0F;
	__asm
	{
		pushad;
		push eax;
		lea eax, [esp + esi + 0xF4];
		push eax;
		push esi;
		call RotateFEWheels;
		popad;

		jmp cExit;
	}
}

static injector::hook_back<void(__fastcall*)(void*, int, void*)> hb_FeCustomizeParts_Ctor;
static injector::hook_back<void(__fastcall*)(void*)> hb_FeCustomizeParts_Dtor;

void __fastcall FeCustomizeParts_Ctor(void* _this, int _edx, void* data)
{
	TargetScrollSpeed = ScrollSpeedMin;
	hb_FeCustomizeParts_Ctor.fun(_this, _edx, data);
}

void __fastcall FeCustomizeParts_Dtor(void* _this)
{
	TargetScrollSpeed = ScrollSpeedMax;
	hb_FeCustomizeParts_Dtor.fun(_this);
}

void Init()
{
	InitGamePositions();
	CIniReader iniReader("NFSMWFELoader.ini");

	for (auto& pos : Positions)
	{
		pos.Preset = iniReader.ReadInteger(pos.IniSection, "Preset", -1);
		if (pos.Preset >= 0)
		{
			char PresetIni[24];
			sprintf(PresetIni, "PRESET_%d", pos.Preset);

			pos.X = iniReader.ReadFloat(PresetIni, (char*)"X", 0);
			pos.Y = iniReader.ReadFloat(PresetIni, (char*)"Y", 0);
			pos.Z = iniReader.ReadFloat(PresetIni, (char*)"Z", 0);

			pos.CarZ = iniReader.ReadFloat(PresetIni, (char*)"CarZ", 0);

			pos.CustomPlatform = iniReader.ReadInteger(PresetIni, (char*)"CustomPlatform", 0) == 1;

			pos.Reflection = iniReader.ReadInteger(PresetIni, (char*)"CarReflection", 1) == 1;

			float rotation = iniReader.ReadFloat(PresetIni, (char*)"CarRotation", -1);
			pos.Rotation = -1;
			if (rotation >= 0)
			{
				if (rotation > 360.0f)
				{
					rotation = rotation - floor(rotation / 360) * 360.0f;
				}
				pos.Rotation = rotation / 360.0f * 65535.0f;
			}
		}
	}
	
	//injector::MakeCALL(0x733963, GetTrackPositionMarker, true);

	bool loadMapInFE = iniReader.ReadInteger("GENERAL", "LoadMapInFE", 0) == 1;
	if (loadMapInFE)
	{
		//injector::WriteMemory(0x006B7E49, "TRACKS\\STREAML2RA.BUN", true);
		//injector::WriteMemory(0x006B7E09, "TRACKS\\L2RA.BUN", true);
	}

	injector::MakeCALL(0x662B8E, FEManager_GetGarageNameFromType, true); // GameFlowLoadGarageScreen
	injector::MakeCALL(0x7B4104, FEManager_GetGarageNameFromType, true); // GarageMainScreen::RefreshBackground

	// Read angles via GarageMainScreen helper functions
	// GarageMainScreen::UpdateRenderingCarParameters
	injector::MakeRangedNOP(0x7A99B5, 0x7A99DC, true);
	injector::MakeCALL(0x7A99B5, GarageMainScreen_GetCarRotationZ, true);

	// GarageMainScreen::HandleRender
	/*
	injector::MakeRangedNOP(0x7A226B, 0x7A2282, true);
	injector::MakeCALL(0x7A226B, GarageMainScreen_GetGeometryXPos, true);
	injector::MakeRangedNOP(0x7A2289, 0x7A22B0, true);
	injector::MakeCALL(0x7A2289, GarageMainScreen_GetGeometryYPos, true);
	injector::MakeRangedNOP(0x7A2289, 0x7A22B0, true);
	injector::MakeCALL(0x7A2289, GarageMainScreen_GetGeometryZPos, true);*/

	LoadCustomPlatform = iniReader.ReadInteger("GENERAL", "LoadCustomPlatform", 0) == 1;
	CustomPlatformPath = iniReader.ReadString("CUSTOM_PLATFORM", "Path", std::string(""));
	if (!CustomPlatformPath.empty() && LoadCustomPlatform)
	{
		if (IsFileExist(CustomPlatformPath.c_str()))
		{
			injector::MakeCALL(0x6DED32, DrawGarageMain, true); // sub_6DE300
			CIniReader hdReflections("NFSMWHDReflections.ini");
			if (hdReflections.ReadInteger("GENERAL", "RealFrontEndReflections", 0) == 1)
			{
				injector::MakeCALL(0x6DE7C5, DrawGarageReflection, true); // sub_6DE300
			}

			injector::MakeJMP(0x7A9BEB, UpdateRenderingCarParametersCave, true);

			hb_FeCustomizeParts_Ctor.fun = injector::MakeCALL(0x571839, FeCustomizeParts_Ctor).get();
			hb_FeCustomizeParts_Dtor.fun = injector::MakeCALL(0x7C01D3, FeCustomizeParts_Dtor).get();

			CustomPlatformPosition.x = iniReader.ReadFloat("CUSTOM_PLATFORM", "X", 0.0f);
			CustomPlatformPosition.y = iniReader.ReadFloat("CUSTOM_PLATFORM", "Y", 0.0f);
			CustomPlatformPosition.z = iniReader.ReadFloat("CUSTOM_PLATFORM", "Z", 0.0f);
			CustomPlatformPosition.w = 1.0f;

			ScrollItems = iniReader.ReadInteger("CUSTOM_PLATFORM", "ScrollItems", 25);

			ScrollSpeedMin = iniReader.ReadFloat("CUSTOM_PLATFORM", "ScrollSpeedMin", 1.0f);
			ScrollSpeedMax = iniReader.ReadFloat("CUSTOM_PLATFORM", "ScrollSpeedMax", 10.0f);

			ScrollSpeed = ScrollSpeedMax;
			TargetScrollSpeed = ScrollSpeedMax;
		}
		else
		{
			MessageBoxA(NULL, "Custom platform geometry not found!", VERSION, MB_ICONERROR);
		}
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
		IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)(base);
		IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);

		if ((base + nt->OptionalHeader.AddressOfEntryPoint + (0x400000 - base)) == 0x7C4040) // Check if .exe file is compatible - Thanks to thelink2012 and MWisBest
		{
			Init();
		}
		else
		{
			MessageBoxA(NULL, "This .exe is not supported.\nPlease use v1.3 English speed.exe (5,75 MB (6.029.312 bytes)).", VERSION, MB_ICONERROR);
			return FALSE;
		}
	}
	break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
