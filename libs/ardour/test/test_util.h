namespace ARDOUR {
	class Session;
}

extern void check_xml (XMLNode *, std::string);
extern ARDOUR::Session* load_session (std::string, std::string);
