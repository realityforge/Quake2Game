/*
===================================================================================================

	ImGui console

	FIXME: you can only browse history after you've submitted something, this is due to the
	placement of con.completionPopup after Con2_Submit

===================================================================================================
*/

#include "cl_local.h"

#include <vector>
#include <string>
#include <array>
#include <algorithm>

#include "imgui.h"
#include "q_imgui_imp.h"

#define MAX_MATCHES		9
#define MAX_NOTIFIES	8

static constexpr uint32 CmdColor = colors::green; // PackColor( 192, 255, 192, 255 )

static cvar_t *con_notifytime;

// Unfinished
class qCircularBuffer
{
	uint m_count;
	uint m_start;
	uint m_end;
};

struct notify_t
{
	char		message[256];
	float		timeLeft;
};

struct editLine_t
{
	char data[256];

	void Clear()
	{
		memset( data, 0, sizeof( data ) );
	}
};

// TODO: Experiment with a circular buffer for the history lines and main buffer
struct console2_t
{
	// the main console buffer, infinitely expanding (bad?)
	std::string				buffer;

	// our input line, and infinitely expanding history (bad?)
	editLine_t				editLine;
	std::vector<editLine_t>	historyLines;
	int						historyPosition = -1;

	// alphabetically sorted lists of matching commands and cvars
	std::vector<std::string_view>	entryMatches;
	uint					beginCvars = 0;				// contains the offset to the cvars in the matches list, so we can sort seperately
	int						completionPosition = -1;

	notify_t				notifies[MAX_NOTIFIES];		// cls.realtime time the line was generated, for transparent notify lines

	bool					scrollToBottom;
	bool					completionPopup;
	bool					wordWrap;
	bool					ignoreEdit;					// when true, CallbackEdit will be ignored once

	// so we can use the console instantly from main
	console2_t()
	{
		// reserve 16384 characters by default
		buffer.reserve( 16384 );
		historyLines.reserve( 32 );

		entryMatches.reserve( 64 );
	}

};

static console2_t con;

//=================================================================================================

void Con2_Print( const char *txt )
{
	con.buffer.append( txt );
}

static int Con2_TextEditCallback( ImGuiInputTextCallbackData *data )
{
	switch ( data->EventFlag )
	{
	case ImGuiInputTextFlags_CallbackEdit:
	{
		if ( !con.ignoreEdit && data->BufTextLen != 0 )
		{
			con.completionPosition = -1;
			// FIXME: THIS IS NEVER SET TO FALSE EVER AGAIN! SO THE USER CANT BROWSE HISTORY!!!!!
			con.completionPopup = true;
		}
		con.ignoreEdit = false;
	}
	break;
	case ImGuiInputTextFlags_CallbackCompletion:
	{
		const char *cmd = Cmd_CompleteCommand( data->Buf );
		if ( !cmd ) {
			cmd = Cvar_CompleteVariable( data->Buf );
		}

		if ( cmd )
		{
			// replace entire buffer
			data->DeleteChars( 0, data->BufTextLen );
			data->InsertChars( data->CursorPos, cmd );
		}
	}
	break;
	case ImGuiInputTextFlags_CallbackHistory:
	{
		if ( con.completionPopup )
		{
			con.ignoreEdit = true;

			switch ( data->EventKey )
			{
			case ImGuiKey_UpArrow:
				if ( con.completionPosition == -1 ) {
					// Start at the end
					con.completionPosition = (int64)con.entryMatches.size() - 1;
				}
				else if ( con.completionPosition > 0 ) {
					// Decrement
					--con.completionPosition;
				}
				break;
			case ImGuiKey_DownArrow:
				if ( con.completionPosition < (int64)con.entryMatches.size() ) {
					++con.completionPosition;
				}
				if ( con.completionPosition == (int64)con.entryMatches.size() ) {
					--con.completionPosition;
				}
				break;
			}

			const char *match_str = con.completionPosition >= 0 ? con.entryMatches[con.completionPosition].data() : "";
			data->DeleteChars( 0, data->BufTextLen );
			data->InsertChars( 0, match_str );
		}
		else if ( !con.historyLines.empty() )
		{
			switch ( data->EventKey )
			{
			case ImGuiKey_UpArrow:
				if ( con.historyPosition == -1 ) {
					// Start at the end
					con.historyPosition = (int64)con.historyLines.size() - 1;
				}
				else if ( con.historyPosition > 0 ) {
					// Decrement
					--con.historyPosition;
				}
				break;
			case ImGuiKey_DownArrow:
				if ( con.historyPosition < (int64)con.historyLines.size() ) {
					++con.historyPosition;
				}
				if ( con.historyPosition == (int64)con.historyLines.size() ) {
					--con.historyPosition;
				}
				break;
			}

			const char *history_str = con.historyPosition >= 0 ? con.historyLines[con.historyPosition].data : "";
			data->DeleteChars( 0, data->BufTextLen );
			data->InsertChars( 0, history_str );
		}
	}
	break;
	}

	return 0;
}

static void Con2_Clear_f()
{
	con.buffer.clear();
}

void Con2_Init()
{
	con_notifytime = Cvar_Get( "con_notifytime", "3", 0 );

	Cmd_AddCommand( "clear", Con2_Clear_f );

	static bool once;

	if ( !once )
	{
		strcpy( con.notifies[0].message, "BROTHER\n" );
		con.notifies[0].timeLeft = SEC2MS( con_notifytime->GetFloat() );
		once = true;
	}
}

/*
========================
Con2_Submit
Public

Submits the text stored in con.editLine, then clears it
========================
*/
static void Con2_Submit()
{
	con.completionPopup = false;
	con.historyPosition = -1;

	// don't push back if the last history content was this very command
	// (mimics cmd)
	if ( con.historyLines.empty() || Q_strcmp( con.historyLines.back().data, con.editLine.data ) != 0 )
	{
		con.historyLines.push_back( con.editLine );
	}

	con.scrollToBottom = true;

	// backslash text are commands, else chat
	if ( con.editLine.data[0] == '\\' || con.editLine.data[0] == '/' )
	{
		// skip the >
		Cbuf_AddText( con.editLine.data + 1 );
	}
	else
	{
		// valid command
		Cbuf_AddText( con.editLine.data );
	}

	Cbuf_AddText( "\n" );

	Com_Printf( "] %s\n", con.editLine.data );

	con.editLine.Clear();
}

int StringSort( const void *p1, const void *p2 )
{
	return Q_strcmp( (const char *)p1, (const char *)p2 );
}

/*
========================
Con2_RegenerateMatches

Finds all matches for a given partially complete string
========================
*/
void Con2_RegenerateMatches( const char *partial )
{
	ASSUME( partial );

	con.entryMatches.clear();

	if ( !partial[0] )
	{
		return;
	}

	strlen_t partialLength = Q_strlen( partial );

	// find command matches
	for ( cmd_function_t *pCmd = cmd_functions; pCmd; pCmd = pCmd->next )
	{
		if ( Q_strncmp( partial, pCmd->name, partialLength ) == 0 )
		{
			con.entryMatches.push_back( pCmd->name );
		}
	}

	std::sort( con.entryMatches.begin(), con.entryMatches.end() );

	// index to the first cvar
	con.beginCvars = static_cast<uint>( con.entryMatches.size() );

	// find cvar matches
	for ( cvar_t *pVar = cvar_vars; pVar; pVar = pVar->pNext )
	{
		if ( Q_strncmp( partial, pVar->GetName(), partialLength ) == 0 )
		{
			con.entryMatches.push_back( pVar->GetName() );
		}
	}

	std::sort( con.entryMatches.begin() + con.beginCvars, con.entryMatches.end() );

	//qsort( matches, numMatches, sizeof( const char * ), StringSort );
}

/*
========================
Con2_ShowConsole

Draws the console using ImGui
========================
*/
void Con2_ShowConsole( bool *pOpen )
{
	ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 330.0f, 250.0f ) );

	ImGui::SetNextWindowSize( ImVec2( 600.0f, 800.0f ), ImGuiCond_FirstUseEver );
	if ( !ImGui::Begin( "Console", pOpen ) )
	{
		ImGui::PopStyleVar();
		ImGui::End();
		return;
	}

	ImGui::PopStyleVar();

	// stash the bottom of this window for the completion prompt
	float windowBottom = ImGui::GetItemRectMax().y;

	if ( ImGui::BeginPopupContextItem() )
	{
		if ( ImGui::MenuItem( "Close Console" ) )
		{
			*pOpen = false;
		}
		ImGui::EndPopup();
	}

	// save the old wordwrap state
	bool wordWrap = con.wordWrap;

	ImGuiWindowFlags scrollFlags = ImGuiWindowFlags_AlwaysAutoResize;
	scrollFlags |= wordWrap ? 0 : ImGuiWindowFlags_HorizontalScrollbar;

	// Reserve enough left-over height for 1 separator + 1 input text
	ImGui::BeginChild( "ScrollingRegion", ImVec2( 0.0f, -ImGui::GetFrameHeightWithSpacing() ), true, scrollFlags );

	if ( ImGui::BeginPopupContextWindow() )
	{
		ImGui::Checkbox( "Word wrap", &con.wordWrap );
		ImGui::EndPopup();
	}

	if ( wordWrap )
	{
		ImGui::PushTextWrapPos( 0.0f );
	}

	if ( !con.buffer.empty() )
	{
		const char *bufferStart = con.buffer.c_str();
		const char *bufferEnd = con.buffer.c_str() + con.buffer.size() - 1;

		const int bufferSize = (int)con.buffer.size() - 1; // skip nul

		if ( bufferSize > 16384 )
		{
			// draw the buffer in 16384 chunks so we can avoid over-running the max vertex count of 65536
			// (16384 * 4 == 65536)
			const char *newStart = bufferStart;

			int chunkSize = 16384;
			int amountLeft = bufferSize;

			ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.0f, 0.0f ) );

			while ( true )
			{
				// starting from the end of the chunk we're supposed to be drawing now, step back to find a newline
				// then use that as the end instead, and subsequently the start of the next chunk
				// this goes under the assumption that there's a newline somewhere... With more than 16384 chars
				// in the buffer this will *always* be true

				const char *newEnd = newStart + chunkSize;

				for ( const char *i = newEnd; i > newStart; --i )
				{
					if ( *i == '\n' )
					{
						chunkSize -= ( chunkSize - static_cast<int>( i - newStart ) ) - 1;
						newEnd = i;
						break;
					}
				}

				ImGui::TextUnformatted( newStart, newEnd );

				amountLeft -= chunkSize;
				newStart += chunkSize;

				if ( amountLeft <= 0 )
				{
					break;
				}

				if ( amountLeft < 16384 )
				{
					chunkSize = amountLeft;
				}
			}

			ImGui::PopStyleVar();
		}
		else
		{
			ImGui::TextUnformatted( bufferStart, bufferEnd );
		}
	}

	if ( wordWrap )
	{
		ImGui::PopTextWrapPos();
	}

	if ( con.scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() )
	{
		ImGui::SetScrollHereY( 1.0f );
	}

	con.scrollToBottom = false;

	ImGui::EndChild();

	// input line
	const ImGuiInputTextFlags inputFlags =
		ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
		ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit;

	bool reclaimFocus = false;

	ImGui::PushItemWidth( ImGui::GetWindowContentRegionWidth() - 64.0f );

	if ( ImGui::InputText( "##Input", con.editLine.data, sizeof( con.editLine.data ), inputFlags, Con2_TextEditCallback, nullptr ) )
	{
		Con2_Submit();

		reclaimFocus = true;
	}

	ImGui::PopItemWidth();

	// auto-focus on window apparition
	ImGui::SetItemDefaultFocus();
	if ( reclaimFocus ) {
		ImGui::SetKeyboardFocusHere( -1 ); // Auto focus previous widget
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Submit" ) )
	{
		Con2_Submit();
	}

	if ( con.completionPopup )
	{
		const ImGuiWindowFlags popFlags =
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;

		ImVec2 popupPosition = ImVec2( ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y );
		//ImGui::SetNextWindowPos( popupPosition );
		//ImGui::SetNextWindowSize( ImVec2( 200.0f, 200.0f ) );

		if ( !con.ignoreEdit )
		{
			Con2_RegenerateMatches( con.editLine.data );
		}

		const uint totalMatches = static_cast<uint>( con.entryMatches.size() );

		// the command popup should only list the first n entries, starting with commands
		if ( totalMatches != 0 && ImGui::Begin( "CommandPopup", nullptr, popFlags ) )
		{
			uint i = 0;

			// loop for the cmds, they're printed green
			ImGui::PushStyleColor( ImGuiCol_Text, CmdColor );
			for ( ; i < (uint)con.beginCvars && i < MAX_MATCHES; ++i )
			{
				ImGui::Selectable( con.entryMatches[i].data() );
			}
			ImGui::PopStyleColor();

			// loop for the cvars
			for ( ; i < (uint)con.entryMatches.size() && i < MAX_MATCHES; ++i )
			{
				ImGui::Selectable( con.entryMatches[i].data() );
			}

			if ( i >= MAX_MATCHES )
			{
				ImGui::Selectable( "...", false, ImGuiSelectableFlags_Disabled );
			}
		}
		if ( totalMatches != 0 )
		{
			ImGui::End();
		}
	}
	/*else
	{
		// TODO: show history?
	}*/

	ImGui::End();
}

/*
===============================================================================

	Notify

===============================================================================
*/

void Con2_ShowNotify()
{
	// only draw in dev mode
	if ( !developer->GetBool() )
	{
		return;
	}

	constexpr float PAD = 40.0f;

	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove;

#if 0
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImVec2 workPos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
		ImVec2 windowPos;
		windowPos.x = workPos.x + PAD;
		windowPos.y = workPos.y + PAD;
		ImGui::SetNextWindowPos( workPos, ImGuiCond_Always );
		windowFlags |= ImGuiWindowFlags_NoMove;
	}
#endif

	ImGui::Begin( "Notify Area", nullptr, windowFlags );

	for ( int i = 0; i < MAX_NOTIFIES; ++i )
	{
		notify_t &notify = con.notifies[i];

		double timeLeft = notify.timeLeft;

		uint32 alpha = 255;

		if ( timeLeft <= 0.5 )
		{
			float fltAlpha = Clamp( timeLeft, 0.0, 0.5 ) / 0.5;

			alpha = static_cast<uint32>( fltAlpha * 255.0 );

			if ( i == 0 && fltAlpha < 0.2 )
			{
				//y -= fontTall * ( 1.0f - f / 0.2f );
			}
		}

		ImGui::TextUnformatted( notify.message );

	}

	ImGui::End();
}

void Con2_ClearNotify()
{
	for ( int i = 0; i < MAX_NOTIFIES; ++i )
	{
		con.notifies[i].timeLeft = 0.0;
	}
}
