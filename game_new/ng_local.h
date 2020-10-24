//=================================================================================================
// Local headers
//=================================================================================================

#pragma once

#include "../common/q_shared.h"

// define GAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
//#define GAME_INCLUDE
#include "../game_shared/game_public.h"

//-------------------------------------------------------------------------------------------------
// ng_main.cpp
//-------------------------------------------------------------------------------------------------

extern game_import_t	gi;
extern game_export_t	globals;

//-------------------------------------------------------------------------------------------------
// ng_svcmds.cpp
//-------------------------------------------------------------------------------------------------

void ServerCommand();

//-------------------------------------------------------------------------------------------------

class CBaseEntity
{
private:

	edict_t*	m_pEdict;
};