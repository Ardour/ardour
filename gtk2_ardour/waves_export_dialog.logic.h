/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

//class WavesExportDialog : public WavesDialog
//{
protected:

	typedef boost::shared_ptr<ARDOUR::ExportHandler> HandlerPtr;
	HandlerPtr _export_handler;

	typedef boost::shared_ptr<ARDOUR::ExportProfileManager> ManagerPtr;
	ManagerPtr _profile_manager;

	typedef boost::shared_ptr<ARDOUR::ExportStatus> StatusPtr;
	StatusPtr _export_status;

	ARDOUR::ExportProfileManager::ExportType _export_type;
	sigc::connection _progress_connection;
	std::stringstream _export_error;
	float _previous_progress; // Needed for gtk bug workaround.


	void init (ARDOUR::Session* session);
	void _show_progress ();
	void _notify_errors (bool force = false);
	void _on_export_button_clicked (WavesButton*);
	void _on_cancel_button_clicked (WavesButton*);
	gint _on_progress_timeout ();


//};
