// cl_inv.c -- client inventory screen

#include "cl_local.h"

/*
========================
CL_ParseInventory
========================
*/
void CL_ParseInventory()
{
	for ( int i = 0; i < MAX_ITEMS; ++i ) {
		cl.inventory[i] = MSG_ReadShort( &net_message );
	}
}

/*
========================
CL_DrawInventory
========================
*/
#define	DISPLAY_ITEMS	17
void CL_DrawInventory()
{
	int			i, j;
	int			num, selected_num, item;
	int			index[MAX_ITEMS];
	char		string[1024];
	int			x, y;
	char		binding[1024];
	const char	*bind;
	int			selected;
	int			top;

	selected = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];

	num = 0;
	selected_num = 0;
	for ( i = 0; i < MAX_ITEMS; i++ )
	{
		if ( i == selected ) {
			selected_num = num;
		}
		if ( cl.inventory[i] )
		{
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS / 2;
	if ( num - top < DISPLAY_ITEMS ) {
		top = num - DISPLAY_ITEMS;
	}
	if ( top < 0 ) {
		top = 0;
	}

	x = ( g_vidDef.width - 256 ) / 2;
	y = ( g_vidDef.height - 240 ) / 2;

	R_DrawPic( x, y + CONCHAR_HEIGHT, "inventory" );

	y += 24;
	x += 24;
	SCR_DrawString( x, y, "hotkey ### item" );
	SCR_DrawString( x, y + CONCHAR_HEIGHT, "------ --- ----" );
	y += 16;
	for ( i = top; i < num && i < top + DISPLAY_ITEMS; i++ )
	{
		item = index[i];
		// search for a binding
		Q_sprintf_s( binding, "use %s", cl.configstrings[CS_ITEMS + item] );
		bind = "";
		for ( j = 0; j < 256; j++ )
		{
			if ( keybindings[j] && Q_stricmp( keybindings[j], binding ) == 0 )
			{
				bind = Key_KeynumToString( j );
				break;
			}
		}

		Q_sprintf_s( string, "%6s %3i %s", bind, cl.inventory[item],
			cl.configstrings[CS_ITEMS + item] );
		uint32 color = colors::defaultText;
		if ( item != selected )
		{
			color = colors::green;
		}
		else // draw a blinky cursor by the selected item
		{
			if ( ( cls.realtime * 10 ) & 1 ) {
				R_DrawChar( x - CONCHAR_WIDTH, y, 15 );
			}
		}
		SCR_DrawStringColor( x, y, string, color );
		y += CONCHAR_HEIGHT;
	}
}
