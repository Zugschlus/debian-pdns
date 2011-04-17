
#line 1 "dnslabeltext.rl"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include "namespaces.hh"


namespace {
void appendSplit(vector<string>& ret, string& segment, char c)
{
  if(segment.size()>254) {
    ret.push_back(segment);
    segment.clear();
  }
  segment.append(1, c);
}
}

vector<string> segmentDNSText(const string& input )
{

#line 26 "dnslabeltext.cc"
static const char _dnstext_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 2, 0, 1, 
	2, 4, 5
};

static const char _dnstext_key_offsets[] = {
	0, 0, 1, 5, 9, 11, 13, 17, 
	21
};

static const char _dnstext_trans_keys[] = {
	34, 34, 92, 32, 126, 34, 92, 48, 
	57, 48, 57, 48, 57, 34, 92, 32, 
	126, 32, 34, 9, 13, 34, 0
};

static const char _dnstext_single_lengths[] = {
	0, 1, 2, 2, 0, 0, 2, 2, 
	1
};

static const char _dnstext_range_lengths[] = {
	0, 0, 1, 1, 1, 1, 1, 1, 
	0
};

static const char _dnstext_index_offsets[] = {
	0, 0, 2, 6, 10, 12, 14, 18, 
	22
};

static const char _dnstext_trans_targs[] = {
	2, 0, 7, 3, 2, 0, 2, 2, 
	4, 0, 5, 0, 6, 0, 7, 3, 
	2, 0, 8, 2, 8, 0, 2, 0, 
	0
};

static const char _dnstext_trans_actions[] = {
	3, 0, 0, 0, 11, 0, 5, 5, 
	7, 0, 7, 0, 7, 0, 9, 9, 
	16, 0, 0, 13, 0, 0, 13, 0, 
	0
};

static const char _dnstext_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 1, 
	1
};

static const int dnstext_start = 1;
static const int dnstext_first_final = 7;
static const int dnstext_error = 0;

static const int dnstext_en_main = 1;


#line 25 "dnslabeltext.rl"


        const char *p = input.c_str(), *pe = input.c_str() + input.length();
        const char* eof = pe;
        int cs;
        char val = 0;

        string segment;
        vector<string> ret;

        
#line 97 "dnslabeltext.cc"
	{
	cs = dnstext_start;
	}

#line 102 "dnslabeltext.cc"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _dnstext_trans_keys + _dnstext_key_offsets[cs];
	_trans = _dnstext_index_offsets[cs];

	_klen = _dnstext_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _dnstext_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	cs = _dnstext_trans_targs[_trans];

	if ( _dnstext_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _dnstext_actions + _dnstext_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 36 "dnslabeltext.rl"
	{ 
                        ret.push_back(segment);
                        segment.clear();
                }
	break;
	case 1:
#line 40 "dnslabeltext.rl"
	{ 
                        segment.clear();
                }
	break;
	case 2:
#line 44 "dnslabeltext.rl"
	{
                  char c = *p;
                  appendSplit(ret, segment, c);
                }
	break;
	case 3:
#line 48 "dnslabeltext.rl"
	{
                  char c = *p;
                  val *= 10;
                  val += c-'0';
                  
                }
	break;
	case 4:
#line 54 "dnslabeltext.rl"
	{
                  appendSplit(ret, segment, val);
                  val=0;
                }
	break;
	case 5:
#line 59 "dnslabeltext.rl"
	{
                  appendSplit(ret, segment, *(p));
                }
	break;
#line 217 "dnslabeltext.cc"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _dnstext_actions + _dnstext_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
#line 36 "dnslabeltext.rl"
	{ 
                        ret.push_back(segment);
                        segment.clear();
                }
	break;
#line 240 "dnslabeltext.cc"
		}
	}
	}

	_out: {}
	}

#line 72 "dnslabeltext.rl"


        if ( cs < dnstext_first_final ) {
                throw runtime_error("Unable to parse DNS TXT '"+input+"'");
        }

        return ret;
};
string segmentDNSLabel(const string& input )
{

#line 260 "dnslabeltext.cc"
static const char _dnslabel_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 2, 3, 0, 2, 3, 
	4
};

static const char _dnslabel_key_offsets[] = {
	0, 0, 4, 8, 10, 12, 16
};

static const char _dnslabel_trans_keys[] = {
	46, 92, 32, 126, 46, 92, 48, 57, 
	48, 57, 48, 57, 46, 92, 32, 126, 
	46, 92, 32, 126, 0
};

static const char _dnslabel_single_lengths[] = {
	0, 2, 2, 0, 0, 2, 2
};

static const char _dnslabel_range_lengths[] = {
	0, 1, 1, 1, 1, 1, 1
};

static const char _dnslabel_index_offsets[] = {
	0, 0, 4, 8, 10, 12, 16
};

static const char _dnslabel_trans_targs[] = {
	6, 2, 1, 0, 1, 1, 3, 0, 
	4, 0, 5, 0, 6, 2, 1, 0, 
	6, 2, 1, 0, 0
};

static const char _dnslabel_trans_actions[] = {
	1, 0, 9, 0, 3, 3, 5, 0, 
	5, 0, 5, 0, 11, 7, 14, 0, 
	1, 0, 9, 0, 0
};

static const int dnslabel_start = 1;
static const int dnslabel_first_final = 6;
static const int dnslabel_error = 0;

static const int dnslabel_en_main = 1;


#line 85 "dnslabeltext.rl"


        const char *p = input.c_str(), *pe = input.c_str() + input.length();
        //const char* eof = pe;
        int cs;
        char val = 0;

        string ret;
        string segment;

        
#line 320 "dnslabeltext.cc"
	{
	cs = dnslabel_start;
	}

#line 325 "dnslabeltext.cc"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _dnslabel_trans_keys + _dnslabel_key_offsets[cs];
	_trans = _dnslabel_index_offsets[cs];

	_klen = _dnslabel_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _dnslabel_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	cs = _dnslabel_trans_targs[_trans];

	if ( _dnslabel_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _dnslabel_actions + _dnslabel_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 96 "dnslabeltext.rl"
	{ 
                        printf("Segment end, segment = '%s'\n", segment.c_str());
                        ret.append(1, (unsigned char)segment.size());
                        ret.append(segment);
                        segment.clear();
                }
	break;
	case 1:
#line 103 "dnslabeltext.rl"
	{
                  printf("'\\%c' ", *p);
                  segment.append(1, *p);
                }
	break;
	case 2:
#line 107 "dnslabeltext.rl"
	{
                  char c = *p;
                  val *= 10;
                  val += c-'0';
                  
                }
	break;
	case 3:
#line 113 "dnslabeltext.rl"
	{
                  printf("_%c_ ", val);
                  segment.append(1, val);
                  val=0;
                }
	break;
	case 4:
#line 119 "dnslabeltext.rl"
	{
                  printf("'%c' ", *p);
                  segment.append(1, *p);
                }
	break;
#line 438 "dnslabeltext.cc"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	_out: {}
	}

#line 134 "dnslabeltext.rl"


        if ( cs < dnslabel_first_final ) {
                throw runtime_error("Unable to parse DNS Label '"+input+"'");
        }
        if(ret.empty() || ret[0] != 0)
          ret.append(1, 0);
        return ret;
};
#if 0
int main()
{
	//char blah[]="\"blah\" \"bleh\" \"bloeh\\\"bleh\" \"\\97enzo\"";
  char blah[]="\"v=spf1 ip4:67.106.74.128/25 ip4:63.138.42.224/28 ip4:65.204.46.224/27 \\013\\010ip4:66.104.217.176/28 \\013\\010ip4:209.48.147.0/27 ~all\"";
  //char blah[]="\"abc \\097\\098 def\"";
  printf("Input: '%s'\n", blah);
	vector<string> res=dnstext(blah);
  cerr<<res.size()<<" segments"<<endl;
  cerr<<res[0]<<endl;
}
#endif
