/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <math.h>
#include <engine/map.h>
#include <engine/kernel.h>

#include <game/mapitems.h>
#include <game/layers.h>
#include <game/collision.h>

#include <game/gamecore.h>
#include <game/animation.h>

#include <engine/shared/config.h>

CCollision::CCollision()
{
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;

	m_pTele = 0;
	m_pSpeedup = 0;
	m_pFront = 0;
	m_pSwitch = 0;
	m_pDoor = 0;
	m_pSwitchers = 0;
	m_pTune = 0;
	
	m_Time = 0.0;
}

void CCollision::Init(class CLayers *pLayers)
{
	if(m_pLayers) m_pLayers->Dest();
	Dest();
	m_NumSwitchers = 0;
	m_pLayers = pLayers;
	m_Width = m_pLayers->GameLayer()->m_Width;
	m_Height = m_pLayers->GameLayer()->m_Height;
	m_pTiles = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->GameLayer()->m_Data));

	if(m_pLayers->TeleLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetUncompressedDataSize(m_pLayers->TeleLayer()->m_Tele);
		if (Size >= m_Width*m_Height*sizeof(CTeleTile))
			m_pTele = static_cast<CTeleTile *>(m_pLayers->Map()->GetData(m_pLayers->TeleLayer()->m_Tele));
	}

	if(m_pLayers->SpeedupLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetUncompressedDataSize(m_pLayers->SpeedupLayer()->m_Speedup);
		if (Size >= m_Width*m_Height*sizeof(CSpeedupTile))
			m_pSpeedup = static_cast<CSpeedupTile *>(m_pLayers->Map()->GetData(m_pLayers->SpeedupLayer()->m_Speedup));
	}

	if(m_pLayers->SwitchLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetUncompressedDataSize(m_pLayers->SwitchLayer()->m_Switch);
		if (Size >= m_Width*m_Height*sizeof(CSwitchTile))
			m_pSwitch = static_cast<CSwitchTile *>(m_pLayers->Map()->GetData(m_pLayers->SwitchLayer()->m_Switch));

		m_pDoor = new CDoorTile[m_Width*m_Height];
		mem_zero(m_pDoor, m_Width * m_Height * sizeof(CDoorTile));
	}
	else
	{
		m_pDoor = 0;
		m_pSwitchers = 0;
	}

	if(m_pLayers->TuneLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetUncompressedDataSize(m_pLayers->TuneLayer()->m_Tune);
		if (Size >= m_Width*m_Height*sizeof(CTuneTile))
			m_pTune = static_cast<CTuneTile *>(m_pLayers->Map()->GetData(m_pLayers->TuneLayer()->m_Tune));
	}

	if(m_pLayers->FrontLayer())
	{
		unsigned int Size = m_pLayers->Map()->GetUncompressedDataSize(m_pLayers->FrontLayer()->m_Front);
		if (Size >= m_Width*m_Height*sizeof(CTile))
			m_pFront = static_cast<CTile *>(m_pLayers->Map()->GetData(m_pLayers->FrontLayer()->m_Front));
	}

	for(int i = 0; i < m_Width*m_Height; i++)
	{
		int Index;
		if(m_pSwitch)
		{
			if(m_pSwitch[i].m_Number > m_NumSwitchers)
				m_NumSwitchers = m_pSwitch[i].m_Number;

			if(m_pSwitch[i].m_Number)
				m_pDoor[i].m_Number = m_pSwitch[i].m_Number;
			else
				m_pDoor[i].m_Number = 0;

			Index = m_pSwitch[i].m_Type;

			if(Index <= TILE_NPH_START)
			{
				if(Index >= TILE_JUMP && Index <= TILE_BONUS)
					m_pSwitch[i].m_Type = Index;
				else
					m_pSwitch[i].m_Type = 0;
			}
		}
	}

	if(m_NumSwitchers)
	{
		m_pSwitchers = new SSwitchers[m_NumSwitchers+1];

		for (int i = 0; i < m_NumSwitchers+1; ++i)
		{
			m_pSwitchers[i].m_Initial = true;
			for (int j = 0; j < MAX_CLIENTS; ++j)
			{
				m_pSwitchers[i].m_Status[j] = true;
				m_pSwitchers[i].m_EndTick[j] = 0;
				m_pSwitchers[i].m_Type[j] = 0;
			}
		}
	}
}

int CCollision::GetTile(int x, int y)
{
	if(!m_pTiles)
		return 0;

	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);
	int pos = Ny * m_Width + Nx;

	if(m_pTiles[pos].m_Index >= TILE_SOLID && m_pTiles[pos].m_Index <= TILE_NOLASER)
		return m_pTiles[pos].m_Index;
	return 0;
}

// TODO: rewrite this smarter!
int CCollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision)
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;
	int ix = 0, iy = 0; // Temporary position for checking collision
	for(int i = 0; i <= End; i++)
	{
		float a = i/(float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		ix = round_to_int(Pos.x);
		iy = round_to_int(Pos.y);

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleHook(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr)
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;
	int ix = 0, iy = 0; // Temporary position for checking collision
	int dx = 0, dy = 0; // Offset for checking the "through" tile
	ThroughOffset(Pos0, Pos1, &dx, &dy);
	for(int i = 0; i <= End; i++)
	{
		float a = i/(float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		ix = round_to_int(Pos.x);
		iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if (g_Config.m_SvOldTeleportHook)
			*pTeleNr = IsTeleport(Index);
		else
			*pTeleNr = IsTeleportHook(Index);
		if(*pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return TILE_TELEINHOOK;
		}

		int hit = 0;
		if(CheckPoint(ix, iy))
		{
			if(!IsThrough(ix, iy, dx, dy, Pos0, Pos1))
				hit = GetCollisionAt(ix, iy);
		}
		else if(IsHookBlocker(ix, iy, Pos0, Pos1))
		{
			hit = TILE_NOHOOK;
		}
		if(hit)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return hit;
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectLineTeleWeapon(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, int *pTeleNr)
{
	float Distance = distance(Pos0, Pos1);
	int End(Distance+1);
	vec2 Last = Pos0;
	int ix = 0, iy = 0; // Temporary position for checking collision
	for(int i = 0; i <= End; i++)
	{
		float a = i/(float)End;
		vec2 Pos = mix(Pos0, Pos1, a);
		ix = round_to_int(Pos.x);
		iy = round_to_int(Pos.y);

		int Index = GetPureMapIndex(Pos);
		if (g_Config.m_SvOldTeleportWeapons)
			*pTeleNr = IsTeleport(Index);
		else
			*pTeleNr = IsTeleportWeapon(Index);
		if(*pTeleNr)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return TILE_TELEINWEAPON;
		}

		if(CheckPoint(ix, iy))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(ix, iy);
		}

		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

// TODO: OPT: rewrite this smarter!
void CCollision::MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces)
{
	if(pBounces)
		*pBounces = 0;

	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;
	if(CheckPoint(Pos + Vel))
	{
		int Affected = 0;
		if(CheckPoint(Pos.x + Vel.x, Pos.y))
		{
			pInoutVel->x *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(CheckPoint(Pos.x, Pos.y + Vel.y))
		{
			pInoutVel->y *= -Elasticity;
			if(pBounces)
				(*pBounces)++;
			Affected++;
		}

		if(Affected == 0)
		{
			pInoutVel->x *= -Elasticity;
			pInoutVel->y *= -Elasticity;
		}
	}
	else
	{
		*pInoutPos = Pos + Vel;
	}
}

bool CCollision::TestBox(vec2 Pos, vec2 Size)
{
	Size *= 0.5f;
	if(CheckPoint(Pos.x-Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y-Size.y))
		return true;
	if(CheckPoint(Pos.x-Size.x, Pos.y+Size.y))
		return true;
	if(CheckPoint(Pos.x+Size.x, Pos.y+Size.y))
		return true;
	return false;
}

void CCollision::MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity)
{
	// do the move
	vec2 Pos = *pInoutPos;
	vec2 Vel = *pInoutVel;

	float Distance = length(Vel);
	int Max = (int)Distance;

	if(Distance > 0.00001f)
	{
		//vec2 old_pos = pos;
		float Fraction = 1.0f/(float)(Max+1);
		for(int i = 0; i <= Max; i++)
		{
			//float amount = i/(float)max;
			//if(max == 0)
				//amount = 0;

			vec2 NewPos = Pos + Vel*Fraction; // TODO: this row is not nice

			if(TestBox(vec2(NewPos.x, NewPos.y), Size))
			{
				int Hits = 0;

				if(TestBox(vec2(Pos.x, NewPos.y), Size))
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					Hits++;
				}

				if(TestBox(vec2(NewPos.x, Pos.y), Size))
				{
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
					Hits++;
				}

				// neither of the tests got a collision.
				// this is a real _corner case_!
				if(Hits == 0)
				{
					NewPos.y = Pos.y;
					Vel.y *= -Elasticity;
					NewPos.x = Pos.x;
					Vel.x *= -Elasticity;
				}
			}

			Pos = NewPos;
		}
	}

	*pInoutPos = Pos;
	*pInoutVel = Vel;
}

// DDRace

void CCollision::Dest()
{
	if(m_pDoor)
		delete[] m_pDoor;
	if(m_pSwitchers)
		delete[] m_pSwitchers;
	m_pTiles = 0;
	m_Width = 0;
	m_Height = 0;
	m_pLayers = 0;
	m_pTele = 0;
	m_pSpeedup = 0;
	m_pFront = 0;
	m_pSwitch = 0;
	m_pTune = 0;
	m_pDoor = 0;
	m_pSwitchers = 0;
}

int CCollision::IsSolid(int x, int y)
{
	int index = GetTile(x,y);
	return index == TILE_SOLID || index == TILE_NOHOOK;
}

bool CCollision::IsThrough(int x, int y, int xoff, int yoff, vec2 pos0, vec2 pos1)
{
	int pos = GetPureMapIndex(x, y);
	if(m_pFront && (m_pFront[pos].m_Index == TILE_THROUGH_ALL || m_pFront[pos].m_Index == TILE_THROUGH_CUT))
		return true;
	if(m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && (
		(m_pFront[pos].m_Flags == ROTATION_0   && pos0.y > pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_90  && pos0.x < pos1.x) ||
		(m_pFront[pos].m_Flags == ROTATION_180 && pos0.y < pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_270 && pos0.x > pos1.x) ))
		return true;
	int offpos = GetPureMapIndex(x+xoff, y+yoff);
	if(m_pTiles[offpos].m_Index == TILE_THROUGH || (m_pFront && m_pFront[offpos].m_Index == TILE_THROUGH))
		return true;
	return false;
}

bool CCollision::IsHookBlocker(int x, int y, vec2 pos0, vec2 pos1)
{
	int pos = GetPureMapIndex(x, y);
	if(m_pTiles[pos].m_Index == TILE_THROUGH_ALL || (m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_ALL))
		return true;
	if(m_pTiles[pos].m_Index == TILE_THROUGH_DIR && (
		(m_pTiles[pos].m_Flags == ROTATION_0   && pos0.y < pos1.y) ||
		(m_pTiles[pos].m_Flags == ROTATION_90  && pos0.x > pos1.x) ||
		(m_pTiles[pos].m_Flags == ROTATION_180 && pos0.y > pos1.y) ||
		(m_pTiles[pos].m_Flags == ROTATION_270 && pos0.x < pos1.x) ))
		return true;
	if(m_pFront && m_pFront[pos].m_Index == TILE_THROUGH_DIR && (
		(m_pFront[pos].m_Flags == ROTATION_0   && pos0.y < pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_90  && pos0.x > pos1.x) ||
		(m_pFront[pos].m_Flags == ROTATION_180 && pos0.y > pos1.y) ||
		(m_pFront[pos].m_Flags == ROTATION_270 && pos0.x < pos1.x) ))
		return true;
	return false;
}

int CCollision::IsWallJump(int Index)
{
	if(Index < 0)
		return 0;

	return m_pTiles[Index].m_Index == TILE_WALLJUMP;
}

int CCollision::IsNoLaser(int x, int y)
{
	return (CCollision::GetTile(x,y) == TILE_NOLASER);
}

int CCollision::IsFNoLaser(int x, int y)
{
	return (CCollision::GetFTile(x,y) == TILE_NOLASER);
}

int CCollision::IsTeleport(int Index)
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEIN)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsEvilTeleport(int Index)
{
	if(Index < 0)
		return 0;
	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINEVIL)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsCheckTeleport(int Index)
{
	if(Index < 0)
		return 0;
	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELECHECKIN)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsCheckEvilTeleport(int Index)
{
	if(Index < 0)
		return 0;
	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELECHECKINEVIL)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTCheckpoint(int Index)
{
	if(Index < 0)
		return 0;

	if(!m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELECHECK)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportWeapon(int Index)
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINWEAPON)
		return m_pTele[Index].m_Number;

	return 0;
}

int CCollision::IsTeleportHook(int Index)
{
	if(Index < 0 || !m_pTele)
		return 0;

	if(m_pTele[Index].m_Type == TILE_TELEINHOOK)
		return m_pTele[Index].m_Number;

	return 0;
}


int CCollision::IsSpeedup(int Index)
{
	if(Index < 0 || !m_pSpeedup)
		return 0;

	if(m_pSpeedup[Index].m_Force > 0)
		return Index;

	return 0;
}

int CCollision::IsTune(int Index)
{
	if(Index < 0 || !m_pTune)
		return 0;

	if(m_pTune[Index].m_Type)
		return m_pTune[Index].m_Number;

	return 0;
}

void CCollision::GetSpeedup(int Index, vec2 *Dir, int *Force, int *MaxSpeed)
{
	if(Index < 0 || !m_pSpeedup)
		return;
	float Angle = m_pSpeedup[Index].m_Angle * (pi / 180.0f);
	*Force = m_pSpeedup[Index].m_Force;
	*Dir = vec2(cos(Angle), sin(Angle));
	if(MaxSpeed)
		*MaxSpeed = m_pSpeedup[Index].m_MaxSpeed;
}

int CCollision::IsSwitch(int Index)
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Type;

	return 0;
}

int CCollision::GetSwitchNumber(int Index)
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0 && m_pSwitch[Index].m_Number > 0)
		return m_pSwitch[Index].m_Number;

	return 0;
}

int CCollision::GetSwitchDelay(int Index)
{
	if(Index < 0 || !m_pSwitch)
		return 0;

	if(m_pSwitch[Index].m_Type > 0)
		return m_pSwitch[Index].m_Delay;

	return 0;
}

int CCollision::IsMover(int x, int y, int* Flags)
{
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);
	int Index = m_pTiles[Ny*m_Width+Nx].m_Index;
	*Flags = m_pTiles[Ny*m_Width+Nx].m_Flags;
	if(Index < 0)
		return 0;
	if (Index == TILE_CP || Index == TILE_CP_F)
		return Index;
	else
		return 0;
}

vec2 CCollision::CpSpeed(int Index, int Flags)
{
	if(Index < 0)
		return vec2(0,0);
	vec2 target;
	if(Index == TILE_CP || Index == TILE_CP_F)
		switch(Flags)
		{
		case ROTATION_0:
			target.x=0;
			target.y=-4;
			break;
		case ROTATION_90:
			target.x=4;
			target.y=0;
			break;
		case ROTATION_180:
			target.x=0;
			target.y=4;
			break;
		case ROTATION_270:
			target.x=-4;
			target.y=0;
			break;
		default:
			target=vec2(0,0);
			break;
		}
	if(Index == TILE_CP_F)
		target*=4;
	return target;
}

int CCollision::GetPureMapIndex(float x, float y)
{
	int Nx = clamp(round_to_int(x)/32, 0, m_Width-1);
	int Ny = clamp(round_to_int(y)/32, 0, m_Height-1);
	return Ny*m_Width+Nx;
}

bool CCollision::TileExists(int Index)
{
	if(Index < 0)
		return false;

	if(m_pTiles[Index].m_Index >= TILE_FREEZE && m_pTiles[Index].m_Index <= TILE_NPH_START)
		return true;
	if(m_pFront && m_pFront[Index].m_Index >= TILE_FREEZE && m_pFront[Index].m_Index  <= TILE_NPH_START)
		return true;
	if(m_pTele && (m_pTele[Index].m_Type == TILE_TELEIN || m_pTele[Index].m_Type == TILE_TELEINEVIL || m_pTele[Index].m_Type == TILE_TELECHECKINEVIL ||m_pTele[Index].m_Type == TILE_TELECHECK || m_pTele[Index].m_Type == TILE_TELECHECKIN))
		return true;
	if(m_pSpeedup && m_pSpeedup[Index].m_Force > 0)
		return true;
	if(m_pDoor && m_pDoor[Index].m_Index)
		return true;
	if(m_pSwitch && m_pSwitch[Index].m_Type)
		return true;
	if(m_pTune && m_pTune[Index].m_Type)
			return true;
	return TileExistsNext(Index);
}

bool CCollision::TileExistsNext(int Index)
{
	if(Index < 0)
		return false;
	int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
	int TileOnTheRight = (Index + 1 < m_Width * m_Height) ? Index + 1 : Index;
	int TileBelow = (Index + m_Width < m_Width * m_Height) ? Index + m_Width : Index;
	int TileAbove = (Index - m_Width > 0) ? Index - m_Width : Index;

	if((m_pTiles[TileOnTheRight].m_Index == TILE_STOP && m_pTiles[TileOnTheRight].m_Flags == ROTATION_270) || (m_pTiles[TileOnTheLeft].m_Index == TILE_STOP && m_pTiles[TileOnTheLeft].m_Flags == ROTATION_90))
		return true;
	if((m_pTiles[TileBelow].m_Index == TILE_STOP && m_pTiles[TileBelow].m_Flags == ROTATION_0) || (m_pTiles[TileAbove].m_Index == TILE_STOP && m_pTiles[TileAbove].m_Flags == ROTATION_180))
		return true;
	if(m_pTiles[TileOnTheRight].m_Index == TILE_STOPA || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pTiles[TileOnTheRight].m_Index == TILE_STOPS || m_pTiles[TileOnTheLeft].m_Index == TILE_STOPS) && m_pTiles[TileOnTheRight].m_Flags|ROTATION_270|ROTATION_90))
		return true;
	if(m_pTiles[TileBelow].m_Index == TILE_STOPA || m_pTiles[TileAbove].m_Index == TILE_STOPA || ((m_pTiles[TileBelow].m_Index == TILE_STOPS || m_pTiles[TileAbove].m_Index == TILE_STOPS) && m_pTiles[TileBelow].m_Flags|ROTATION_180|ROTATION_0))
		return true;
	if(m_pFront)
	{
		if(m_pFront[TileOnTheRight].m_Index == TILE_STOPA || m_pFront[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pFront[TileOnTheRight].m_Index == TILE_STOPS || m_pFront[TileOnTheLeft].m_Index == TILE_STOPS) && m_pFront[TileOnTheRight].m_Flags|ROTATION_270|ROTATION_90))
			return true;
		if(m_pFront[TileBelow].m_Index == TILE_STOPA || m_pFront[TileAbove].m_Index == TILE_STOPA || ((m_pFront[TileBelow].m_Index == TILE_STOPS || m_pFront[TileAbove].m_Index == TILE_STOPS) && m_pFront[TileBelow].m_Flags|ROTATION_180|ROTATION_0))
			return true;
		if((m_pFront[TileOnTheRight].m_Index == TILE_STOP && m_pFront[TileOnTheRight].m_Flags == ROTATION_270) || (m_pFront[TileOnTheLeft].m_Index == TILE_STOP && m_pFront[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pFront[TileBelow].m_Index == TILE_STOP && m_pFront[TileBelow].m_Flags == ROTATION_0) || (m_pFront[TileAbove].m_Index == TILE_STOP && m_pFront[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	if(m_pDoor)
	{
		if(m_pDoor[TileOnTheRight].m_Index == TILE_STOPA || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPA || ((m_pDoor[TileOnTheRight].m_Index == TILE_STOPS || m_pDoor[TileOnTheLeft].m_Index == TILE_STOPS) && m_pDoor[TileOnTheRight].m_Flags|ROTATION_270|ROTATION_90))
			return true;
		if(m_pDoor[TileBelow].m_Index == TILE_STOPA || m_pDoor[TileAbove].m_Index == TILE_STOPA || ((m_pDoor[TileBelow].m_Index == TILE_STOPS || m_pDoor[TileAbove].m_Index == TILE_STOPS) && m_pDoor[TileBelow].m_Flags|ROTATION_180|ROTATION_0))
			return true;
		if((m_pDoor[TileOnTheRight].m_Index == TILE_STOP && m_pDoor[TileOnTheRight].m_Flags == ROTATION_270) || (m_pDoor[TileOnTheLeft].m_Index == TILE_STOP && m_pDoor[TileOnTheLeft].m_Flags == ROTATION_90))
			return true;
		if((m_pDoor[TileBelow].m_Index == TILE_STOP && m_pDoor[TileBelow].m_Flags == ROTATION_0) || (m_pDoor[TileAbove].m_Index == TILE_STOP && m_pDoor[TileAbove].m_Flags == ROTATION_180))
			return true;
	}
	return false;
}

int CCollision::GetMapIndex(vec2 Pos)
{
	int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
	int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
	int Index = Ny*m_Width+Nx;

	if(TileExists(Index))
		return Index;
	else
		return -1;
}

std::list<int> CCollision::GetMapIndices(vec2 PrevPos, vec2 Pos, unsigned MaxIndices)
{
	std::list< int > Indices;
	float d = distance(PrevPos, Pos);
	int End(d + 1);
	if(!d)
	{
		int Nx = clamp((int)Pos.x / 32, 0, m_Width - 1);
		int Ny = clamp((int)Pos.y / 32, 0, m_Height - 1);
		int Index = Ny * m_Width + Nx;

		if(TileExists(Index))
		{
			Indices.push_back(Index);
			return Indices;
		}
		else
			return Indices;
	}
	else
	{
		float a = 0.0f;
		vec2 Tmp = vec2(0, 0);
		int Nx = 0;
		int Ny = 0;
		int Index,LastIndex = 0;
		for(int i = 0; i < End; i++)
		{
			a = i/d;
			Tmp = mix(PrevPos, Pos, a);
			Nx = clamp((int)Tmp.x / 32, 0, m_Width - 1);
			Ny = clamp((int)Tmp.y / 32, 0, m_Height - 1);
			Index = Ny * m_Width + Nx;
			if(TileExists(Index) && LastIndex != Index)
			{
				if(MaxIndices && Indices.size() > MaxIndices)
					return Indices;
				Indices.push_back(Index);
				LastIndex = Index;
			}
		}

		return Indices;
	}
}

vec2 CCollision::GetPos(int Index)
{
	if(Index < 0)
		return vec2(0,0);

	int x = Index%m_Width;
	int y = Index/m_Width;
	return vec2(x*32+16, y*32+16);
}

int CCollision::GetTileIndex(int Index)
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Index;
}

int CCollision::GetFTileIndex(int Index)
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Index;
}

int CCollision::GetTileFlags(int Index)
{
	if(Index < 0)
		return 0;
	return m_pTiles[Index].m_Flags;
}

int CCollision::GetFTileFlags(int Index)
{
	if(Index < 0 || !m_pFront)
		return 0;
	return m_pFront[Index].m_Flags;
}

int CCollision::GetIndex(int Nx, int Ny)
{
	return m_pTiles[Ny*m_Width+Nx].m_Index;
}

int CCollision::GetIndex(vec2 PrevPos, vec2 Pos)
{
	float Distance = distance(PrevPos, Pos);

	if(!Distance)
	{
		int Nx = clamp((int)Pos.x/32, 0, m_Width-1);
		int Ny = clamp((int)Pos.y/32, 0, m_Height-1);

		if ((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny*m_Width+Nx].m_Force > 0))
		{
			return Ny*m_Width+Nx;
		}
	}

	float a = 0.0f;
	vec2 Tmp = vec2(0, 0);
	int Nx = 0;
	int Ny = 0;

	for(float f = 0; f < Distance; f++)
	{
		a = f/Distance;
		Tmp = mix(PrevPos, Pos, a);
		Nx = clamp((int)Tmp.x/32, 0, m_Width-1);
		Ny = clamp((int)Tmp.y/32, 0, m_Height-1);
		if ((m_pTele) ||
			(m_pSpeedup && m_pSpeedup[Ny*m_Width+Nx].m_Force > 0))
		{
			return Ny*m_Width+Nx;
		}
	}

	return -1;
}

int CCollision::GetFIndex(int Nx, int Ny)
{
	if(!m_pFront) return 0;
	return m_pFront[Ny*m_Width+Nx].m_Index;
}

int CCollision::GetFTile(int x, int y)
{
	if(!m_pFront)
	return 0;
	int Nx = clamp(x/32, 0, m_Width-1);
	int Ny = clamp(y/32, 0, m_Height-1);
	if(m_pFront[Ny*m_Width+Nx].m_Index == TILE_DEATH
		|| m_pFront[Ny*m_Width+Nx].m_Index == TILE_NOLASER)
		return m_pFront[Ny*m_Width+Nx].m_Index;
	else
		return 0;
}

int CCollision::Entity(int x, int y, int Layer)
{
	if((0 > x || x >= m_Width) || (0 > y || y >= m_Height))
	{
		char aBuf[12];
		switch (Layer)
		{
			case LAYER_GAME:
				str_format(aBuf,sizeof(aBuf), "Game");
				break;
			case LAYER_FRONT:
				str_format(aBuf,sizeof(aBuf), "Front");
				break;
			case LAYER_SWITCH:
				str_format(aBuf,sizeof(aBuf), "Switch");
				break;
			case LAYER_TELE:
				str_format(aBuf,sizeof(aBuf), "Tele");
				break;
			case LAYER_SPEEDUP:
				str_format(aBuf,sizeof(aBuf), "Speedup");
				break;
			case LAYER_TUNE:
				str_format(aBuf,sizeof(aBuf), "Tune");
				break;
			default:
				str_format(aBuf,sizeof(aBuf), "Unknown");
		}
		dbg_msg("collision","something is VERY wrong with the %s layer please report this at https://github.com/ddnet/ddnet, you will need to post the map as well and any steps that u think may have led to this", aBuf);
		return 0;
	}
	switch (Layer)
	{
		case LAYER_GAME:
			return m_pTiles[y*m_Width+x].m_Index - ENTITY_OFFSET;
		case LAYER_FRONT:
			return m_pFront[y*m_Width+x].m_Index - ENTITY_OFFSET;
		case LAYER_SWITCH:
			return m_pSwitch[y*m_Width+x].m_Type - ENTITY_OFFSET;
		case LAYER_TELE:
			return m_pTele[y*m_Width+x].m_Type - ENTITY_OFFSET;
		case LAYER_SPEEDUP:
			return m_pSpeedup[y*m_Width+x].m_Type - ENTITY_OFFSET;
		case LAYER_TUNE:
			return m_pTune[y*m_Width+x].m_Type - ENTITY_OFFSET;
		default:
			return 0;
			break;
	}
}

void CCollision::SetCollisionAt(float x, float y, int id)
{
	int Nx = clamp(round_to_int(x)/32, 0, m_Width-1);
	int Ny = clamp(round_to_int(y)/32, 0, m_Height-1);

	m_pTiles[Ny * m_Width + Nx].m_Index = id;
}

void CCollision::SetDCollisionAt(float x, float y, int Type, int Flags, int Number)
{
	if(!m_pDoor)
		return;
	int Nx = clamp(round_to_int(x)/32, 0, m_Width-1);
	int Ny = clamp(round_to_int(y)/32, 0, m_Height-1);

	m_pDoor[Ny * m_Width + Nx].m_Index = Type;
	m_pDoor[Ny * m_Width + Nx].m_Flags = Flags;
	m_pDoor[Ny * m_Width + Nx].m_Number = Number;
}

int CCollision::GetDTileIndex(int Index)
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	return m_pDoor[Index].m_Index;
}

int CCollision::GetDTileNumber(int Index)
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	if(m_pDoor[Index].m_Number) return m_pDoor[Index].m_Number;
	return 0;
}

int CCollision::GetDTileFlags(int Index)
{
	if(!m_pDoor || Index < 0 || !m_pDoor[Index].m_Index)
		return 0;
	return m_pDoor[Index].m_Flags;
}

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *Ox, int *Oy)
{
	float x = Pos0.x - Pos1.x;
	float y = Pos0.y - Pos1.y;
	if (fabs(x) > fabs(y))
	{
		if (x < 0)
		{
			*Ox = -32;
			*Oy = 0;
		}
		else
		{
			*Ox = 32;
			*Oy = 0;
		}
	}
	else
	{
		if (y < 0)
		{
			*Ox = 0;
			*Oy = -32;
		}
		else
		{
			*Ox = 0;
			*Oy = 32;
		}
	}
}

int CCollision::IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(float f = 0; f < d; f++)
	{
		float a = f/d;
		vec2 Pos = mix(Pos0, Pos1, a);
		int Nx = clamp(round_to_int(Pos.x)/32, 0, m_Width-1);
		int Ny = clamp(round_to_int(Pos.y)/32, 0, m_Height-1);
		if(GetIndex(Nx, Ny) == TILE_SOLID
			|| GetIndex(Nx, Ny) == TILE_NOHOOK
			|| GetIndex(Nx, Ny) == TILE_NOLASER
			|| GetFIndex(Nx, Ny) == TILE_NOLASER)
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if (GetFIndex(Nx, Ny) == TILE_NOLASER)	return GetFCollisionAt(Pos.x, Pos.y);
			else return GetCollisionAt(Pos.x, Pos.y);

		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectNoLaserNW(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(float f = 0; f < d; f++)
	{
		float a = f/d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)) || IsFNoLaser(round_to_int(Pos.x), round_to_int(Pos.y)))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(IsNoLaser(round_to_int(Pos.x), round_to_int(Pos.y))) return GetCollisionAt(Pos.x, Pos.y);
			else return  GetFCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IntersectAir(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision)
{
	float d = distance(Pos0, Pos1);
	vec2 Last = Pos0;

	for(float f = 0; f < d; f++)
	{
		float a = f/d;
		vec2 Pos = mix(Pos0, Pos1, a);
		if(IsSolid(round_to_int(Pos.x), round_to_int(Pos.y)) || (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFTile(round_to_int(Pos.x), round_to_int(Pos.y))))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			if(!GetTile(round_to_int(Pos.x), round_to_int(Pos.y)) && !GetFTile(round_to_int(Pos.x), round_to_int(Pos.y)))
				return -1;
			else
				if (!GetTile(round_to_int(Pos.x), round_to_int(Pos.y))) return GetTile(round_to_int(Pos.x), round_to_int(Pos.y));
				else return GetFTile(round_to_int(Pos.x), round_to_int(Pos.y));
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
}

int CCollision::IsCheckpoint(int Index)
{
	if(Index < 0)
		return -1;

	int z = m_pTiles[Index].m_Index;
	if(z >= 35 && z <= 59)
		return z-35;
	return -1;
}

int CCollision::IsFCheckpoint(int Index)
{
	if(Index < 0 || !m_pFront)
		return -1;

	int z = m_pFront[Index].m_Index;
	if(z >= 35 && z <= 59)
		return z-35;
	return -1;
}

int CCollision::GetZoneHandle(const char* pName)
{
	if(!m_pLayers->ZoneGroup())
		return -1;
	
	int Handle = m_Zones.size();
	m_Zones.add(array<int>());
	
	array<int>& LayerList = m_Zones[Handle];
	
	char aLayerName[12];
	for(int l = 0; l < m_pLayers->ZoneGroup()->m_NumLayers; l++)
	{
		CMapItemLayer *pLayer = m_pLayers->GetLayer(m_pLayers->ZoneGroup()->m_StartLayer+l);
		
		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			CMapItemLayerTilemap *pTLayer = (CMapItemLayerTilemap *)pLayer;
			IntsToStr(pTLayer->m_aName, sizeof(aLayerName)/sizeof(int), aLayerName);
			if(str_comp(pName, aLayerName) == 0)
				LayerList.add(l);
		}
		else if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
			IntsToStr(pQLayer->m_aName, sizeof(aLayerName)/sizeof(int), aLayerName);
			if(str_comp(pName, aLayerName) == 0)
				LayerList.add(l);
		}
	}
	
	return Handle;
}

/* TEEUNIVERSE  BEGIN *************************************************/

inline bool SameSide(const vec2& l0, const vec2& l1, const vec2& p0, const vec2& p1)
{
	vec2 l0l1 = l1-l0;
	vec2 l0p0 = p0-l0;
	vec2 l0p1 = p1-l0;
	
	return sign(l0l1.x*l0p0.y - l0l1.y*l0p0.x) == sign(l0l1.x*l0p1.y - l0l1.y*l0p1.x);
}

//t0, t1 and t2 are position of triangle vertices
inline vec3 BarycentricCoordinates(const vec2& t0, const vec2& t1, const vec2& t2, const vec2& p)
{
    vec2 e0 = t1 - t0;
    vec2 e1 = t2 - t0;
    vec2 e2 = p - t0;
    
    float d00 = dot(e0, e0);
    float d01 = dot(e0, e1);
    float d11 = dot(e1, e1);
    float d20 = dot(e2, e0);
    float d21 = dot(e2, e1);
    float denom = d00 * d11 - d01 * d01;
    
    vec3 bary;
    bary.x = (d11 * d20 - d01 * d21) / denom;
    bary.y = (d00 * d21 - d01 * d20) / denom;
    bary.z = 1.0f - bary.x - bary.y;
    
    return bary;
}

//t0, t1 and t2 are position of triangle vertices
inline bool InsideTriangle(const vec2& t0, const vec2& t1, const vec2& t2, const vec2& p)
{
    vec3 bary = BarycentricCoordinates(t0, t1, t2, p);
    return (bary.x >= 0.0f && bary.y >= 0.0f && bary.x + bary.y < 1.0f);
}

//t0, t1 and t2 are position of quad vertices
inline bool InsideQuad(const vec2& q0, const vec2& q1, const vec2& q2, const vec2& q3, const vec2& p)
{
	if(SameSide(q1, q2, p, q0))
		return InsideTriangle(q0, q1, q2, p);
	else
		return InsideTriangle(q1, q2, q3, p);
}

/* TEEUNIVERSE END ****************************************************/

static void Rotate(vec2 *pCenter, vec2 *pPoint, float Rotation)
{
	float x = pPoint->x - pCenter->x;
	float y = pPoint->y - pCenter->y;
	pPoint->x = (x * cosf(Rotation) - y * sinf(Rotation) + pCenter->x);
	pPoint->y = (x * sinf(Rotation) + y * cosf(Rotation) + pCenter->y);
}

int CCollision::GetZoneValueAt(int ZoneHandle, float x, float y)
{
	if(!m_pLayers->ZoneGroup())
		return 0;
	
	if(ZoneHandle < 0 || ZoneHandle >= m_Zones.size())
		return 0;
	
	int Index = 0;
	
	for(int i = 0; i < m_Zones[ZoneHandle].size(); i++)
	{
		int l = m_Zones[ZoneHandle][i];
		
		CMapItemLayer *pLayer = m_pLayers->GetLayer(m_pLayers->ZoneGroup()->m_StartLayer+l);
		if(pLayer->m_Type == LAYERTYPE_TILES)
		{
			CMapItemLayerTilemap *pTLayer = (CMapItemLayerTilemap *)pLayer;
			
			CTile *pTiles = (CTile *) m_pLayers->Map()->GetData(pTLayer->m_Data);
			
			int Nx = clamp(round_to_int(x)/32, 0, pTLayer->m_Width-1);
			int Ny = clamp(round_to_int(y)/32, 0, pTLayer->m_Height-1);
			
			int TileIndex = (pTiles[Ny*pTLayer->m_Width+Nx].m_Index > 128 ? 0 : pTiles[Ny*pTLayer->m_Width+Nx].m_Index);
			if(TileIndex > 0)
				Index = TileIndex;
		}
		else if(pLayer->m_Type == LAYERTYPE_QUADS)
		{
			CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
			
			const CQuad *pQuads = (const CQuad *) m_pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

			for(int q = 0; q < pQLayer->m_NumQuads; q++)
			{
				vec2 Position(0.0f, 0.0f);
				float Angle = 0.0f;
				if(pQuads[q].m_PosEnv >= 0)
				{
					GetAnimationTransform(m_Time, pQuads[q].m_PosEnv, m_pLayers, Position, Angle);
				}
				
				vec2 p0 = Position + vec2(fx2f(pQuads[q].m_aPoints[0].x), fx2f(pQuads[q].m_aPoints[0].y));
				vec2 p1 = Position + vec2(fx2f(pQuads[q].m_aPoints[1].x), fx2f(pQuads[q].m_aPoints[1].y));
				vec2 p2 = Position + vec2(fx2f(pQuads[q].m_aPoints[2].x), fx2f(pQuads[q].m_aPoints[2].y));
				vec2 p3 = Position + vec2(fx2f(pQuads[q].m_aPoints[3].x), fx2f(pQuads[q].m_aPoints[3].y));
				
				if(Angle != 0)
				{
					vec2 center(fx2f(pQuads[q].m_aPoints[4].x), fx2f(pQuads[q].m_aPoints[4].y));
					Rotate(&center, &p0, Angle);
					Rotate(&center, &p1, Angle);
					Rotate(&center, &p2, Angle);
					Rotate(&center, &p3, Angle);
				}
				
				if(InsideQuad(p0, p1, p2, p3, vec2(x, y)))
				{
					Index = pQuads[q].m_ColorEnvOffset;
				}
			}
		}
	}
	
	return Index;
}
