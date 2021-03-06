/*
*   Copyright (C) 2017 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "WAV2AMBE.h"

#include "WAVFileReader.h"
#include "AMBEFileWriter.h"

#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
char* optarg = NULL;
int optind = 1;

int getopt(int argc, char* const argv[], const char* optstring)
{
	if ((optind >= argc) || (argv[optind][0] != '-') || (argv[optind][0] == 0))
		return -1;

	int opt = argv[optind][1];
	const char *p = strchr(optstring, opt);

	if (p == NULL) {
		return '?';
	}

	if (p[1] == ':') {
		optind++;
		if (optind >= argc)
			return '?';

		optarg = argv[optind];
		optind++;
	}

	return opt;
}
#else
#include <unistd.h>
#endif

int main(int argc, char** argv)
{
	float amplitude = 1.0F;
	std::string signature;
	AMBE_MODE mode = MODE_DSTAR;
	bool fec = true;
	std::string port = "/dev/ttyUSB0";
	unsigned int speed = 230400U;
	bool reset = false;

	int c;
	while ((c = ::getopt(argc, argv, "a:f:g:m:p:rs:")) != -1) {
		switch (c) {
		case 'a':
			amplitude = float(::atof(optarg));
			break;
		case 'f':
			fec = ::atoi(optarg) != 0;
			break;
		case 'g':
			signature = std::string(optarg);
			break;
		case 'm':
			if (::strcmp(optarg, "dstar") == 0)
				mode = MODE_DSTAR;
			else if (::strcmp(optarg, "dmr") == 0)
				mode = MODE_DMR;
			else if (::strcmp(optarg, "ysf") == 0)
				mode = MODE_YSF;
			else if (::strcmp(optarg, "p25") == 0)
				mode = MODE_P25;
			else
				mode = MODE_UNKNOWN;
			break;
		case 'p':
			port = std::string(optarg);
			break;
		case 'r':
			reset = true;
			break;
		case 's':
			speed = (unsigned int)::atoi(optarg);
			break;
		case '?':
			break;
		default:
			fprintf(stderr, "Usage: WAV2AMBE [-a amplitude] [-g <signature>] [-m dstar|dmr|ysf|p25] [-f 0|1] [-p <port>] [-s <speed>] [-r] <input> <output>\n");
			break;
		}
	}

	if (optind > (argc - 2)) {
		fprintf(stderr, "Usage: WAV2AMBE [-a amplitude] [-g <signature>] [-m dstar|dmr|ysf|p25] [-f 0|1] [-p <port>] [-s <speed>] [-r] <input> <output>\n");
		return 1;
	}

	if (mode == MODE_UNKNOWN) {
		::fprintf(stderr, "WAV2AMBE: unknown mode specified\n");
		return 1;
	}

	if (mode == MODE_DSTAR && !fec) {
		::fprintf(stderr, "WAV2AMBE: incompatible mode and FEC settings\n");
		return 1;
	}

	if (mode == MODE_YSF && !fec) {
		::fprintf(stderr, "WAV2AMBE: incompatible mode and FEC settings\n");
		return 1;
	}

	CWAV2AMBE* WAV2AMBE = new CWAV2AMBE(signature, mode, fec, port, speed, amplitude, reset, std::string(argv[argc - 2]), std::string(argv[argc - 1]));

	int ret = WAV2AMBE->run();

	delete WAV2AMBE;

	return ret;
}

CWAV2AMBE::CWAV2AMBE(const std::string& signature, AMBE_MODE mode, bool fec, const std::string& port, unsigned int speed, float amplitude, bool reset, const std::string& input, const std::string& output) :
m_signature(signature),
m_mode(mode),
m_fec(fec),
m_port(port),
m_speed(speed),
m_amplitude(amplitude),
m_reset(reset),
m_input(input),
m_output(output)
{
}

CWAV2AMBE::~CWAV2AMBE()
{
}

int CWAV2AMBE::run()
{
	CWAVFileReader reader(m_input, AUDIO_BLOCK_SIZE);
	bool ret = reader.open();
	if (!ret)
		return 1;

	if (reader.getSampleRate() != AUDIO_SAMPLE_RATE) {
		::fprintf(stderr, "WAV2AMBE: input file has the wrong sample rate\n");
		reader.close();
		return 1;
	}

	if (reader.getChannels() > 1U) {
		::fprintf(stderr, "WAV2AMBE: input file has too many channels\n");
		reader.close();
		return 1;
	}

	CAMBEFileWriter writer(m_output, m_signature);
	ret = writer.open();
	if (!ret) {
		reader.close();
		return 1;
	}

	CDV3000SerialController controller(m_port, m_speed, m_mode, m_fec, m_amplitude, m_reset, &reader, &writer);
	ret = controller.open();
	if (!ret) {
		writer.close();
		reader.close();
		return 1;
	}

	controller.process();

	controller.close();
	writer.close();
	reader.close();

	return 0;
}
