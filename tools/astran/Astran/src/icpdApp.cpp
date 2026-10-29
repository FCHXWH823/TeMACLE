//---------------------------------------------------------------------------
//
// Name:        icpdApp.cpp
// Author:      Adriel Mota Ziesemer Junior
// Created:     20/9/2007 19:13:08
// Description: 
//
//---------------------------------------------------------------------------

#include "icpdApp.h"

DECLARE_APP(icpdFrmApp)
IMPLEMENT_APP_NO_MAIN(icpdFrmApp);

#include <cstdlib>
#include <cstring>
#include <editline/readline.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Line editing and a command history that survives the session.
//
// The shell loop below read with plain getline, so an interactive session had
// no editing and no up-arrow recall.  libedit ships with macOS and exposes a
// readline-compatible subset of the API, so this costs no new dependency --
// only -ledit at link time (see nbproject/Makefile-Release.mk).
//
// Used only when stdin is a terminal.  A script runner invokes
// `Astran -s file` with stdin closed, and that path stays on getline: its logs
// then remain byte-identical to before, and nothing writes a history file on
// behalf of a batch run.
// ---------------------------------------------------------------------------
static string historyPath()
{
    const char* home = getenv("HOME");
    return string(home ? home : ".") + "/.astran_history";
}

static bool nextCommand(string& cmd, bool interactive, string& prev)
{
    if (!interactive) {
        cout << "astran> " << flush;
        return bool(getline(cin, cmd));
    }

    char* line = readline("astran> ");
    if (!line) return false;                   // EOF, i.e. ctrl-D
    cmd = line;
    free(line);

    // A blank line and an immediate repeat are not worth recalling, and
    // keeping them out is what makes the history usable after a long session.
    if (!cmd.empty() && cmd != prev) {
        add_history(cmd.c_str());
        prev = cmd;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Shell mode bypasses wxEntry entirely on macOS.
//
// This class fakes a console app by overriding Initialize() to call
// wxAppConsole::Initialize() while still deriving from the GUI wxApp.  That
// works on GTK.  It does not work on the Cocoa port of wx 3.1.3 and later,
// where wxApp::CallOnInit() runs OnInit from inside [NSApplication run]:
// control never reaches the console branch below, the process blocks in the
// Cocoa event loop, and nothing is printed.  Confirmed by stack sample:
//
//     wxEntry -> wxApp::CallOnInit -> -[NSApplication run] -> ... -> mach_msg2
//
// Nothing in the shell path needs a wxApp -- DesignMng is plain C++ -- so the
// fix is to not build one.  A GUI launch still goes through wxEntry unchanged.
// ---------------------------------------------------------------------------
static int runShellSession(const char* cmdFile)
{
    setlocale(LC_ALL, "C");

    if (cmdFile && *cmdFile) {
        ifstream probe(cmdFile);
        if (!probe) {
            cout << "ERROR: File '" << cmdFile << "' doesn't exist" << endl;
            return 1;
        }
    }

    DesignMng designmng;
    string cmd;

    const char* astran_path = getenv("ASTRAN_PATH");
    string astran_cfg = string(astran_path ? astran_path : ".") + "/bin/astran.cfg";
    ifstream afile(astran_cfg.c_str());
    if (afile) {
        cmd = string("read ") + astran_cfg;
        cout << "astran> " << cmd << endl;
        designmng.readCommand(cmd);
    }

    if (cmdFile && *cmdFile) {
        cmd = string("read ") + cmdFile;
        cout << "astran> " << cmd << endl;
        designmng.readCommand(cmd);
    }

    // EOF ends the session.  The original loops on `while (true)`, which spins
    // forever once stdin is closed -- the state a script runner leaves it in.
    const bool interactive = isatty(STDIN_FILENO);
    const string hist = historyPath();
    if (interactive) {
        stifle_history(1000);
        read_history(hist.c_str());            // absent on the first run
    }

    string prev;
    while (nextCommand(cmd, interactive, prev))
        designmng.readCommand(cmd);

    if (interactive)
        write_history(hist.c_str());
    cout << endl;
    return 0;
}

int main(int argc, char** argv)
{
    const char* cmdFile = NULL;
    bool shell = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--shell")) shell = true;
        else if (argv[i][0] != '-') cmdFile = argv[i];
    }
    if (shell) return runShellSession(cmdFile);
    return wxEntry(argc, argv);
}

bool icpdFrmApp::Initialize(int& argc, wchar_t **argv) { 

    static const wxCmdLineEntryDesc desc[] = { 
        { wxCMD_LINE_SWITCH, wxT_2("s"), wxT_2("shell"), wxT_2("Run in shell mode") }, 
        // { wxCMD_LINE_SWITCH, wxT_2("c"), wxT_2("commands"), wxT_2("Show all available commands") },
        { wxCMD_LINE_PARAM, NULL, NULL, wxT_2("FILENAME"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_NONE }
    }; 

    wxCmdLineParser parser(desc, argc, argv); 
    if (parser.Parse(true) != 0) { 
        exit(1); 
    } 
    
    gui_enabled = !parser.Found(wxT_2("shell"));

    // cout << " STAT:" << parser.Parse(true) << endl;
    // cout << " FILE:" << parser.GetParam(0).mb_str() << endl;
    // cout << " CONT:" << parser.GetParamCount() << endl;

    if (parser.GetParamCount() == 1) {
        cmdFilename = parser.GetParam(0);
    }

    if (gui_enabled) { 
        return wxApp::Initialize(argc, argv); 
    } else { 
        return wxAppConsole::Initialize(argc, argv); 
    }
} 

bool icpdFrmApp::OnInit()
{
    setlocale(LC_ALL, "C");

    ifstream ifile(cmdFilename.mb_str());
    if ((!ifile) && (!cmdFilename.empty())) {
        cout << "ERROR: File \'" << cmdFilename.mb_str() << "\' doesn't exist" << endl;
        return false;
    }

    if (!gui_enabled) 
    {
        DesignMng designmng;
        string cmd;

        wxString astran_path;
        ::wxGetEnv(wxT_2("ASTRAN_PATH"), &astran_path);
        string astran_cfg;
        astran_cfg = "astran.cfg";
        astran_cfg = string(wxString(astran_path).mb_str()) + "/bin/astran.cfg";        

        // By default load astran.cfg
        ifstream afile(astran_cfg.c_str());
        if (afile) 
        {
            cmd = string("read ") + astran_cfg;
            cout << "astran> " << cmd << endl;
            designmng.readCommand(cmd); 
        }

        // Read astran_script
        if (ifile) 
        {
            cmd = string("read ") + string(wxString(cmdFilename).mb_str());
            cout << "astran> " << cmd << endl;
            designmng.readCommand(cmd);
        }

        while (true) {
            cout << "astran> ";
            getline (cin, cmd);
            designmng.readCommand(cmd);
        }
    } 
    else
    {
        wxInitAllImageHandlers();
        IcpdFrm *frame = new IcpdFrm(NULL);
        SetTopWindow(frame);
        frame->Show();
    }

    return true;
}

int icpdFrmApp::OnExit(){
    return EXIT_SUCCESS;
}

void icpdFrmApp::CleanUp()
{ 
    if (gui_enabled) { 
        wxApp::CleanUp(); 
    } else { 
        wxAppConsole::CleanUp(); 
    } 
} 

HybridTraits *icpdFrmApp::CreateTraits()
{ 
    return new HybridTraits(gui_enabled); 
} 
