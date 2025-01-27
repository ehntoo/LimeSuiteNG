#ifndef __pnlMiniLog__
#define __pnlMiniLog__

#include <deque>
#include <utility> //pair
#include "limesuiteng/Logger.h"

class wxChoice;
class wxButton;
class wxTextCtrl;
class wxCheckBox;
class wxTextAttr;

#include <wx/panel.h>

class pnlMiniLog : public wxPanel
{
  public:
    pnlMiniLog(wxWindow* parent,
        wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTAB_TRAVERSAL);
    void OnBtnClearClicked(wxCommandEvent& event);
    void OnShowFullLog(wxCommandEvent& event);
    void onLogLvlChange(wxCommandEvent& event);
    //// end generated class members
    void HandleMessage(wxCommandEvent& event);
    void OnUpdateGUI(wxUpdateUIEvent& event);

  private:
    std::deque<wxString> mAllMessages;
    std::deque<std::pair<lime::LogLevel, wxString>> mMessageList;
    int log_level;

  protected:
    wxTextCtrl* txtMessageField;
    wxButton* btnClear;
    wxButton* btnFullLog;
    wxChoice* choiceLogLvl;
};

#endif // __pnlMiniLog__
