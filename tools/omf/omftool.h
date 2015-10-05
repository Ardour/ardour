#ifndef __ardour_omftool__
#define __ardour_omftool__

#include <vector>
#include <string>
#include <cstdio>

#include <stdint.h>
#include <sqlite3.h>

class XMLNode;

class OMF {
public:
	OMF ();
	~OMF ();

	int init ();
	int load (const std::string&);
	int create_xml ();

	void set_version (int);
	void set_session_name (const std::string&);
	void set_sample_rate (int);

	struct SourceInfo {
	    int channels;
	    int sample_rate;
	    uint64_t length;
	    XMLNode* node;

	    SourceInfo (int chn, int sr, uint64_t l, XMLNode* n)
	    : channels (chn), sample_rate (sr), length (l), node (n) {}
	};

private:
	bool bigEndian;
	int64_t id_counter;
	FILE* file;
	sqlite3* db;
	int version;
	std::string base_dir;
	std::string session_name;
	std::vector<std::string> audiofile_path_vector;
	int      sample_rate; /* audio samples per second */
	double   frame_rate;  /* time per video frame */
	XMLNode* session;
	XMLNode* sources;
	XMLNode* routes;
	XMLNode* regions;
	XMLNode* playlists;
	XMLNode* diskstreams;
	XMLNode* locations;
	XMLNode* options;

	XMLNode* new_region_node ();
	XMLNode* new_source_node ();
	XMLNode* new_route_node ();
	XMLNode* new_playlist_node ();
	XMLNode* new_diskstream_node ();

	typedef std::map<std::string,SourceInfo*> KnownSources;
	KnownSources known_sources;

	SourceInfo* get_known_source (const char*);
	char* read_name (size_t offset, size_t length);
	bool get_offset_and_length (const char* offstr, const char* lenstr, uint32_t& offset, uint32_t& len);
	void name_types ();
	void add_id (XMLNode*);
	void set_route_node_channels (XMLNode* route, int in, int out, bool send_to_master);
	bool get_audio_info (const std::string& path);
	void set_region_sources (XMLNode*, SourceInfo*);
	void legalize_name (std::string&);

	uint16_t e16(uint16_t x)
	{
		if (bigEndian)
			return (x>>8)
				| (x<<8);
		else
			return x;
	}

	uint32_t e32(uint32_t x)
	{
		if (bigEndian)
			return (x>>24) |
				((x<<8) & 0x00FF0000) |
				((x>>8) & 0x0000FF00) |
				(x<<24);
		else
			return x;
	}

	uint64_t e64(uint64_t x)
	{
		if (bigEndian)
			return (x>>56) |
				((x<<40) & 0x00FF000000000000) |
				((x<<24) & 0x0000FF0000000000) |
				((x<<8)  & 0x000000FF00000000) |
				((x>>8)  & 0x00000000FF000000) |
				((x>>24) & 0x0000000000FF0000) |
				((x>>40) & 0x000000000000FF00) |
				(x<<56);
		else
			return x;
	}

};

#endif /* __ardour_omftool__ */
