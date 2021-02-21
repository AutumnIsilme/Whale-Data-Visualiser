#include "Log.h"
#include "Window.h"
#include "Application.h"
#include "Timer.h"

#include "Shaders.h"
#include "TextureShaders.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <GLFW/glfw3.h>

#include <nfd.h>

#include <imgui.h>
#include "ImGuiImpl.h"

#include <implot.h>

#include <stb_image.h>

#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <cstdio>
#include <thread>

_getShader(vs_texture);
_getShader(fs_texture);

Window mainWindow;

u32 WIDTH = 1280;
u32 HEIGHT = 720;

const int SAMPLES_PER_SECOND = 25;

const double half_pi = 1.5707963267948966;
const double pi = 3.141592653589793;
const double two_pi = 6.283185307179586;

std::vector<float> depth_data;
std::vector<float> depth_differences;
std::vector<float> depth_velocity_data;
std::vector<float> depth_velocity_differences;
std::vector<float> depth_acceleration_data;
std::vector<float> pitch_data;
std::vector<float> roll_data;
std::vector<float> yaw_data;
std::vector<float> yaw_differences;
std::vector<float> yaw_velocity_data;
std::vector<float> x_data;
u8 just_loaded_depth = 0;
u8 just_loaded_pitch = 0;
u8 just_loaded_roll = 0;
u8 just_loaded_yaw = 0;

bool yaw_data_use_velocity = false;
bool just_changed_use_yaw_velocity = false;
const char* yaw_data_absolute_string = "Heading data: Absolute";
const char* yaw_data_velocity_string = "Heading data: Rate";

struct PosColTexVertex
{
	float x;
	float y;
	float z;
	uint32_t abgr;
	float u;
	float v;
};

static PosColTexVertex pointerVertices[] =
{
	{ 0.0f,  2.0f,  0.0f, 0xff0000ff, 0.0f, 0.0f },
	{ 0.0f, -1.0f,  1.0f, 0xff00ff00, 0.0f, 0.0f },
	{ 0.5f, -1.0f, -1.0f, 0xff00ffff, 0.0f, 0.0f },
	{-0.5f, -1.0f, -1.0f, 0xffff0000, 0.0f, 0.0f },
};

static const uint16_t pointerTriList[] =
{
	0, 1, 2,
	0, 3, 1,
	0, 2, 3,
	1, 3, 2,
};

static PosColTexVertex s_XZAxes[] =
{
	{-3.0f, -3.0f, -3.0f,   0xffffffff,   0.0f, 0.0f},
	{-3.0f,  3.0f, -3.0f,   0xffffffff,   0.0f, 1.0f},
	{ 3.0f,  3.0f, -3.0f,   0xffffffff,   1.0f, 1.0f},
	{ 3.0f, -3.0f, -3.0f,   0xffffffff,   1.0f, 0.0f}
};

static PosColTexVertex s_XYAxes[] =
{
	{-3.0f,  3.0f, -3.0f,   0xffffffff,   0.0f, 0.0f},
	{-3.0f,  3.0f,  3.0f,   0xffffffff,   0.0f, 1.0f},
	{ 3.0f,  3.0f,  3.0f,   0xffffffff,   1.0f, 1.0f},
	{ 3.0f,  3.0f, -3.0f,   0xffffffff,   1.0f, 0.0f}
};

static PosColTexVertex s_YZAxes[] =
{
	{-3.0f, -3.0f, -3.0f,   0xffffffff,   0.0f, 0.0f},
	{-3.0f, -3.0f,  3.0f,   0xffffffff,   0.0f, 1.0f},
	{-3.0f,  3.0f,  3.0f,   0xffffffff,   1.0f, 1.0f},
	{-3.0f,  3.0f, -3.0f,   0xffffffff,   1.0f, 0.0f}
};

static const uint16_t s_AxesTris[] =
{
	0, 1, 2,
	0, 2, 3,
	0, 2, 1,
	0, 3, 2
};

static const uint16_t s_cubeTriList[] =
{
	0, 2, 1, // 0
	1, 2, 3,
	4, 5, 6, // 2
	5, 7, 6,
	0, 4, 2, // 4
	4, 6, 2,
	1, 3, 5, // 6
	5, 3, 7,
	0, 1, 4, // 8
	4, 1, 5,
	2, 6, 3, // 10
	6, 7, 3,
};

bgfx::TextureHandle play_button_texture_play;
bgfx::TextureHandle play_button_texture_pause;
bgfx::TextureHandle settings_button_texture;
bgfx::TextureHandle xy_axes_texture;
bgfx::TextureHandle xz_axes_texture;
bgfx::TextureHandle yz_axes_texture;
bgfx::TextureHandle whale_texture;

std::string filepaths[4] = { "", "", "", "" };
bool single_cache = false;

bool show_err_msg = false;
std::string err_msg = "";

float mark_0 = 0, mark_1 = 0;

/* Called when the window size changes and at startup to ensure clear colour is set and window size is known */
void reset(s32 width, s32 height)
{
	WIDTH = width;
	HEIGHT = height;

	bgfx::reset(width, height, BGFX_RESET_VSYNC);
	imguiReset(width, height);

	bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000FF, 1.0f, 0);
	bgfx::setViewRect(0, 0, 0, WIDTH, HEIGHT);
}

enum class DataType
{
	DEPTH = 0,
	PITCH,
	ROLL,
	YAW,

	COUNT
};

/* Load data from a file for a passed data type/buffer.
 * Called by LoadData and LoadDataCache. */
int ActuallyLoadData(std::vector<float>* data, DataType type, const char* filepath, double* x_max, bool do_set_x_data)
{
	auto load_timer = timer_create();
	FILE* file = fopen(filepath, "rb");
	if (file == NULL)
	{
		LOG_ERROR("Failed to open file \"{0}\".", filepath);
		show_err_msg = true;
		err_msg = "Failed to open file";
		return 1;
	}

	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	// Allocate space then read into that space the contents of the file
	char* mem = (char*)malloc(filesize + 1);
	defer{ free(mem); };
	fread(mem, 1, filesize, file);
	mem[filesize] = '\n';
	fclose(file);

	// @TODO: Maybe hold onto the old data to not replace it if the input is bad?
	data->clear();
	if (type == DataType::DEPTH)
	{
		depth_differences.clear();
		depth_velocity_data.clear();
		depth_velocity_differences.clear();
		depth_acceleration_data.clear();
	}
	if (type == DataType::YAW)
		yaw_velocity_data.clear();

	// @TODO: X data compatability check
	/*bool do_set_x_data = false;
	if (x_data.size() == 0)
		do_set_x_data = true;*/

	const int sign_corrections[4] = { -1, 1, 1, 1 };

	// Loop through the data in the file
	for (u32 index = 0; index < filesize; index++)
	{
		// At every iteration, we know we are at the start of a number
		u32 start = index;
		// Loop to the end of the number
		while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
		{
			index++;
		}
		// There's an unexpected character here, so stop
		if (mem[index] != '\n')
		{
			LOG_ERROR("Malformed data input: Expected number or newline, got {0} at {1}.", mem[index], index);

			show_err_msg = true;
			err_msg = "Malformed data input: Expected number or newline";
			// @TODO: Maybe save the old data and restore if something breaks?
			data->clear();
			if (type == DataType::DEPTH)
			{
				depth_differences.clear();
				depth_velocity_data.clear();
				depth_velocity_differences.clear();
				depth_acceleration_data.clear();
			}
			if (type == DataType::YAW)
				yaw_velocity_data.clear();
			if (do_set_x_data)
				x_data.clear();
			return 2;
		}
		// Extract the number that we now know the start and length of, and turn it into a number
		data->push_back(sign_corrections[(unsigned)type] * std::stof(std::string(&mem[start], index - start)));
		// If we haven't filled the x_data buffer yet (just a sequence of numbers up to the length of the data), add the next number
		if (do_set_x_data)
			x_data.push_back(x_data.size());

		if (type == DataType::DEPTH)
		{
			if (data->size() == 1)
				depth_differences.push_back(0);
			else
				depth_differences.push_back(data->at(data->size() - 1) - data->at(data->size() - 2));
		}
		else if (type == DataType::YAW)
		{
			if (data->size() == 1)
				yaw_differences.push_back(0);
			else
			{
				auto first = data->at(data->size() - 2);
				auto second = data->at(data->size() - 1);
				if (first < -half_pi && second > half_pi)
				{
					second -= two_pi;
				}
				else if (second < -half_pi && first > half_pi)
				{
					first -= two_pi;
				}
				yaw_differences.push_back(second - first);
			}
		}
	}

	static const int average_width = 150;
	// Set flags for which graph to adjust the zoom level of, and log success, and calculate derivatives if applicable
	switch (type)
	{
	case DataType::DEPTH:
	{

		auto t = timer_create();

		/*/
		for (int i = 0; i < data.size(); i++)
		{
			if (i > average_width / 2 + (average_width % 2) + 1 && i < data.size() - average_width / 2 + (average_width % 2))
			{
				// Calculate velocity
				float sum = 0;
				for (int j = i - average_width / 2; j < i + average_width / 2 + (average_width % 2); j++)
				{
					sum += depth_differences[j];
				}
				depth_velocity_data.push_back(sum / (float) average_width);
			} else
			{
				depth_velocity_data.push_back(0);
			}
		}
		/*/

		{
			float sum = 0;
			for (int i = 0; i < average_width; i++)
			{
				sum += depth_differences[i];
			}
			int i = 0;
			for (; i < average_width / 2; i++)
			{
				depth_velocity_data.push_back(0);
				depth_velocity_differences.push_back(0);
			}

			depth_velocity_data.push_back(sum / (float)average_width);
			for (; i < data->size() - average_width / 2 + (average_width % 2) - 1; i++)
			{
				sum += depth_differences[i + (average_width / 2) + (average_width % 2) - 1] - depth_differences[i - (average_width / 2)];
				depth_velocity_data.push_back(SAMPLES_PER_SECOND * sum / (float)average_width);
				depth_velocity_differences.push_back(depth_velocity_data[depth_velocity_data.size() - 1] - depth_velocity_data[depth_velocity_data.size() - 2]);
			}
			
			for (; i < data->size(); i++)
			{
				depth_velocity_data.push_back(0);
				depth_velocity_differences.push_back(0);
			}
		}

		LOG_INFO("Time elapsed: {}", timer_elapsed(&t));

		timer_reset(&t);

		{
			float sum = 0;
			for (int i = 0; i < average_width; i++)
			{
				sum += depth_velocity_differences[i];
			}
			int i = 0;
			for (; i < average_width / 2; i++)
				depth_acceleration_data.push_back(0);
			depth_acceleration_data.push_back(sum / (float)average_width);
			for (; i < data->size() - average_width / 2 + (average_width % 2) - 1; i++)
			{
				sum += depth_velocity_differences[i + (average_width / 2) + (average_width % 2) - 1] - depth_velocity_differences[i - (average_width / 2)];
				depth_acceleration_data.push_back(SAMPLES_PER_SECOND * sum / (float)average_width);
			}
			for (; i < data->size(); i++)
				depth_acceleration_data.push_back(0);
		}

		LOG_INFO("Time elapsed 2: {}", timer_elapsed(&t));

		/*if (depth_velocity_data.size() > 2)
			depth_velocity_data[0] = depth_velocity_data[1];*/
		LOG_INFO("Loaded depth data from \"{0}\".", filepath);
		just_loaded_depth = 3;
	} break;
	case DataType::PITCH:
		LOG_INFO("Loaded pitch data from \"{0}\".", filepath);
		just_loaded_pitch = 3;
		break;
	case DataType::ROLL:
		LOG_INFO("Loaded roll data from \"{0}\".", filepath);
		just_loaded_roll = 3;
		break;
	case DataType::YAW:
	{
		{
			float sum = 0;
			for (int i = 0; i < average_width; i++)
			{
				sum += yaw_differences[i];
			}
			int i = 0;
			for (; i < average_width / 2; i++)
				yaw_velocity_data.push_back(0);
			yaw_velocity_data.push_back(sum / (float)average_width);
			for (; i < data->size() - average_width / 2 + (average_width % 2) - 1; i++)
			{
				sum += yaw_differences[i + (average_width / 2) + (average_width % 2) - 1] - yaw_differences[i - (average_width / 2)];
				yaw_velocity_data.push_back(SAMPLES_PER_SECOND * sum / (float)average_width);
			}
			for (; i < data->size(); i++)
				yaw_velocity_data.push_back(0);
		}

		LOG_INFO("Loaded heading data from \"{0}\".", filepath);
		just_loaded_yaw = 3;
	} break;

	case DataType::COUNT:
	default:
		LOG_WARN("Unrecognised data type provided, loaded from \"{0}\".", filepath);
		break;
	}

	*x_max = x_data.size();

	LOG_INFO("Data load time: {}", timer_elapsed(&load_timer));

	return 0;
}

int expect(char** data, char e)
{
	if (**data != e)
	{
		return 1;
	}
	(*data)++;
	return 0;
}

/* Load data from a saved cache (currently just a list of filenames so you don't have to open each individually. */
void LoadDataCache(double* x_max)
{
	char* filepath = NULL;
	nfdresult_t open_result = NFD_OpenDialog(NULL, NULL, &filepath);
	defer{ free(filepath); };
	if (open_result == NFD_CANCEL)
	{
		return;
	} else if (open_result == NFD_ERROR)
	{
		LOG_ERROR("Open dialog failed: {}", NFD_GetError());
		return;
	}
	auto t = timer_create();

	FILE* file = fopen(filepath, "rb");
	if (file == NULL)
	{
		LOG_ERROR("Failed to open file \"{0}\".", filepath);
		show_err_msg = true;
		err_msg = "Failed to open file";
		return;
	}

	fseek(file, 0, SEEK_END);
	u32 filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	// Allocate space then read into that space the contents of the file
	char* mem = (char*)malloc(filesize + 1);
	defer{ free(mem); };
	fread(mem, 1, filesize, file);
	mem[filesize] = '\n';
	fclose(file);

	char* head = mem;
	// We expect a directive "@type {}" at the top of the file
	if (expect(&head, '@') || expect(&head, 't') || expect(&head, 'y') || expect(&head, 'p') || expect(&head, 'e') || expect(&head, ' '))
	{
		if (*mem != '@') goto default_load;
		while (*head != ' ' && head - mem < 15) head++;
		LOG_ERROR("Malformed data input: Unrecognised header tag ({})", std::string(mem, head - mem));
		show_err_msg = true;
		err_msg = "Malformed data input: Unrecognised header tag";
		return;
	}

	switch (expect(&head, 'c'))
	{
	case 1:
	{
		if (expect(&head, 's') || expect(&head, 'e') || expect(&head, 'c') || expect(&head, 't') || expect(&head, 'i') || expect(&head, 'o') || expect(&head, 'n'))
		{
			while (*head != '\n' && head - mem < 15) head++;
			LOG_ERROR("Malformed data input: Unrecognised header type ({})", std::string(mem, head - mem));
			show_err_msg = true;
			err_msg = "Malformed data input: Unrecognised header type";
			return;
		}
		if (expect(&head, '\n'))
		{
			LOG_ERROR("Malformed data input: Expected newline, got {}", *head);
			show_err_msg = true;
			err_msg = "Malformed data input: Expected newline";
			return;
		}

		depth_data.clear();
		depth_differences.clear();
		depth_velocity_data.clear();
		depth_velocity_differences.clear();
		depth_acceleration_data.clear();

		pitch_data.clear();
		roll_data.clear();
		yaw_data.clear();

		x_data.clear();

		just_loaded_depth = true;
		just_loaded_pitch = true;
		just_loaded_roll = true;
		just_loaded_yaw = true;
		single_cache = true;
		mark_0 = 0;
		mark_1 = 0;

		// Loop through the data in the file
		for (u32 index = head - mem; index < filesize; index++)
		{
			// At every iteration, we know we are at the start of a number
			u32 start = index;
			// Loop to the end of the number
			while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
			{
				index++;
				head++;
			}
			// Extract the number that we now know the start and length of, and turn it into a number
			depth_data.push_back(std::stof(std::string(&mem[start], index - start)));

			if (expect(&head, ',') || expect(&head, ' '))
			{
				LOG_ERROR("Malformed data input: Expected comma or space, got {0} at {1}.", mem[index], index);
				show_err_msg = true;
				err_msg = "Malformed data input: Expected newline";
				// @TODO: Maybe save the old data and restore if something breaks?
				depth_data.clear();
				depth_differences.clear();
				depth_velocity_data.clear();
				depth_velocity_differences.clear();
				depth_acceleration_data.clear();
				return;
			}
			index += 2;

			// At every iteration, we know we are at the start of a number
			start = index;
			// Loop to the end of the number
			while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
			{
				index++;
				head++;
			}
			// Extract the number that we now know the start and length of, and turn it into a number
			depth_velocity_data.push_back(std::stof(std::string(&mem[start], index - start)));

			if (expect(&head, ',') || expect(&head, ' '))
			{
				LOG_ERROR("Malformed data input: Expected comma or space, got {0} at {1}.", mem[index], index);
				show_err_msg = true;
				err_msg = "Malformed data input: Expected newline";
				depth_data.clear();
				depth_differences.clear();
				depth_velocity_data.clear();
				depth_velocity_differences.clear();
				depth_acceleration_data.clear();
				return;
			}
			index += 2;

			// At every iteration, we know we are at the start of a number
			start = index;
			// Loop to the end of the number
			while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
			{
				index++;
				head++;
			}
			// Extract the number that we now know the start and length of, and turn it into a number
			depth_acceleration_data.push_back(std::stof(std::string(&mem[start], index - start)));

			if (expect(&head, ',') || expect(&head, ' '))
			{
				LOG_ERROR("Malformed data input: Expected comma or space, got {0} at {1}.", mem[index], index);
				show_err_msg = true;
				err_msg = "Malformed data input: Expected newline";
				depth_data.clear();
				depth_differences.clear();
				depth_velocity_data.clear();
				depth_velocity_differences.clear();
				depth_acceleration_data.clear();
				return;
			}
			index += 2;

			// At every iteration, we know we are at the start of a number
			start = index;
			// Loop to the end of the number
			while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
			{
				index++;
				head++;
			}
			// Extract the number that we now know the start and length of, and turn it into a number
			pitch_data.push_back(std::stof(std::string(&mem[start], index - start)));

			if (expect(&head, ',') || expect(&head, ' '))
			{
				LOG_ERROR("Malformed data input: Expected comma or space, got {0} at {1}.", mem[index], index);
				show_err_msg = true;
				err_msg = "Malformed data input: Expected newline";
				depth_data.clear();
				depth_differences.clear();
				depth_velocity_data.clear();
				depth_velocity_differences.clear();
				depth_acceleration_data.clear();
				return;
			}
			index += 2;

			// At every iteration, we know we are at the start of a number
			start = index;
			// Loop to the end of the number
			while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
			{
				index++;
				head++;
			}
			// Extract the number that we now know the start and length of, and turn it into a number
			roll_data.push_back(std::stof(std::string(&mem[start], index - start)));

			if (expect(&head, ',') || expect(&head, ' '))
			{
				LOG_ERROR("Malformed data input: Expected comma or space, got {0} at {1}.", mem[index], index);
				show_err_msg = true;
				err_msg = "Malformed data input: Expected newline";
				depth_data.clear();
				depth_differences.clear();
				depth_velocity_data.clear();
				depth_velocity_differences.clear();
				depth_acceleration_data.clear();
				return;
			}
			index += 2;

			// At every iteration, we know we are at the start of a number
			start = index;
			// Loop to the end of the number
			while (('0' <= mem[index] && mem[index] <= '9') || mem[index] == '.' || mem[index] == '-' || mem[index] == 'e')
			{
				index++;
				head++;
			}
			// Extract the number that we now know the start and length of, and turn it into a number
			yaw_data.push_back(std::stof(std::string(&mem[start], index - start)));

			// If we haven't filled the x_data buffer yet (just a sequence of numbers up to the length of the data), add the next number
			x_data.push_back(x_data.size());

			if (expect(&head, '\n'))
			{
				LOG_ERROR("Malformed data input: Expected newline, got {0} at {1}.", mem[index], index);
				show_err_msg = true;
				err_msg = "Malformed data input: Expected newline";
				depth_data.clear();
				depth_differences.clear();
				depth_velocity_data.clear();
				depth_velocity_differences.clear();
				depth_acceleration_data.clear();
				return;
			}
		}
	} break;
	case 0:
	{
		if (expect(&head, 'a') || expect(&head, 'c') || expect(&head, 'h') || expect(&head, 'e'))
		{
			while (*head != '\n' && head - mem < 15) head++;
			show_err_msg = true;
			LOG_ERROR("Malformed data input: Unrecognised header type ({})", std::string(mem, head - mem));
			err_msg = "Malformed data input: Unrecognised header type";
			return;
		}

	default_load:
		x_data.clear();

		just_loaded_depth = true;
		just_loaded_pitch = true;
		just_loaded_roll = true;
		just_loaded_yaw = true;
		single_cache = false;
		mark_0 = 0;
		mark_1 = 0;

		std::thread threads[4];
		int results[4] = { 0 };
		std::string fnames[4];

		// Start the loop through the cache file
		bool done[4] = { 0 };
		while (head < mem + filesize)
		{
			// Save the type of the data (indicated by a number at the start of an entry
			char type = *head;
			head += 2;
			u64 count = 0;
			// Go to the end of the line
			while (*head != '\n' && *head != '\r')
			{
				head++;
				count++;
			}
			// Extract the file name
			char* fname = (char*)malloc(count + 1);
			// Free the allocated memory when we exit the scope
			defer{ free(fname); };
			memcpy(fname, head - count, count);
			fname[count] = 0;

			auto load = [](std::vector<float>* data, DataType type, std::string fname, double* x_max, bool set_x_data, int* result)
			{
				//LOG_INFO("Thread lambda called with {}, {}, {}, {}", type, fname, *x_max, set_x_data);
				*result = ActuallyLoadData(data, type, fname.c_str(), x_max, set_x_data);
			};

			switch (type)
			{
			case '0':
			{
				// If we have already filled out this data from this cache, complain, otherwise actually load the data
				if (!done[0])
				{
					done[0] = true;
					fnames[0] = std::string(fname);
					threads[0] = std::thread(load, &depth_data, DataType::DEPTH, fnames[0], x_max, true, &results[0]);
				} else
				{
					LOG_WARN("Multiple data of the same type (Depth) present at filename {}", fname);
				}
			} break;
			case '1':
			{
				if (!done[1])
				{
					done[1] = true;
					fnames[1] = std::string(fname);
					threads[1] = std::thread(load, &pitch_data, DataType::PITCH, fnames[1], x_max, false, &results[1]);
				} else
				{
					LOG_WARN("Multiple data of the same type (Pitch) present at filename {}", fname);
				}
			} break;
			case '2':
			{
				if (!done[2])
				{
					done[2] = true;
					fnames[2] = std::string(fname);
					threads[2] = std::thread(load, &roll_data, DataType::ROLL, fnames[2], x_max, false, &results[2]);
					//int result = ActuallyLoadData(roll_data, DataType::ROLL, fname, x_max);
				} else
				{
					LOG_WARN("Multiple data of the same type (Roll) present at filename {}", fname);
				}
			} break;
			case '3':
			{
				if (!done[3])
				{
					done[3] = true;
					fnames[3] = std::string(fname);
					threads[3] = std::thread(load, &yaw_data, DataType::YAW, fnames[3], x_max, false, &results[3]);
					//int result = ActuallyLoadData(yaw_data, DataType::YAW, fname, x_max);
				} else
				{
					LOG_WARN("Multiple data of the same type (Heading) present at filename {}", fname);
				}
			} break;
			default:
			{
				LOG_ERROR("Line malformed: no type indicator present");
				show_err_msg = true;
			} break;
			}

			// Go forward until we reach the next entry
			while (*head == '\n' || *head == '\r')
			{
				head++;
			}
		}
		for (int i = 0; i < 4; i++)
		{
			if (threads[i].joinable())
				threads[i].join();
			if (!results[i])
				filepaths[i] = fnames[i];
			else
			{
				show_err_msg = true;
			}
		}
		if (show_err_msg)
		{
			err_msg = "Data cache load failed. See log for more details.";
			depth_data.clear();
			depth_velocity_data.clear();
			depth_acceleration_data.clear();
			pitch_data.clear();
			roll_data.clear();
			yaw_data.clear();
		}
		else if (x_data.size() == 0)
		{
			LOG_CRITICAL("Threading is broken!");
			exit(1);
		}
	} break;
	}
	LOG_INFO("Loaded data cache from {}, taking {} seconds.", filepath, timer_elapsed(&t));
}

void ExportDataSection(double x_min, double x_max)
{
	if (depth_data.size() < x_max || pitch_data.size() < x_max || yaw_data.size() < x_max || roll_data.size() < x_max || x_min < 0 || x_min >= x_max)
	{
		LOG_ERROR("Bounds check failed on export attempt");
		return;
	}
	char* filepath = NULL;
	nfdresult_t open_result = NFD_SaveDialog(NULL, NULL, &filepath);
	if (open_result == NFD_CANCEL)
	{
		return;
	} else if (open_result == NFD_ERROR)
	{
		LOG_ERROR("Open dialog failed: {}", NFD_GetError());
		return;
	}

	// Open the file in write mode
	FILE* file = fopen(filepath, "wb");
	if (file == NULL)
	{
		LOG_ERROR("Failed to open file \"{0}\".", filepath);
		return;
	}

	fprintf(file, "@type section\n");

	u64 x1 = x_min;
	u64 x2 = x_max;
	for (u64 i = x1; i < x2; i++)
	{
		fprintf(file, "%f, %f, %f, %f, %f, %f\n", depth_data[i], depth_velocity_data[i], depth_acceleration_data[i], pitch_data[i], roll_data[i], yaw_data[i]);
	}
	fclose(file);
	LOG_INFO("Exported data section to {}", filepath);
	free(filepath);
}

/* Save a data cache to disk (Actually just a list of filenames to be loaded at one time later) */
void SaveDataCache()
{
	if (single_cache)
	{
		ExportDataSection(0, x_data.size());
		return;
	}
	char* filepath = NULL;
	nfdresult_t open_result = NFD_SaveDialog(NULL, NULL, &filepath);
	if (open_result == NFD_CANCEL)
	{
		return;
	} else if (open_result == NFD_ERROR)
	{
		LOG_ERROR("Open dialog failed: {}", NFD_GetError());
		return;
	}

	// Open the file in write mode
	FILE* file = fopen(filepath, "wb");
	if (file == NULL)
	{
		LOG_ERROR("Failed to open file \"{0}\".", filepath);
		show_err_msg = true;
		err_msg = "Failed to open file";
		return;
	}

	fprintf(file, "@type cache\n");

	for (int i = 0; i < 4; i++)
	{
		if (filepaths[i].size() != 0)
		{
			// Write the data type and path to the file
			fprintf(file, "%d %s\n", i, filepaths[i].c_str());
		}
	}
	fclose(file);
	LOG_INFO("Saved data cache to {}", filepath);
	free(filepath);
}

// @TODO: Move this to a worker thread so it doesn't block the main window?
/* Load a specific data type from a file */
void LoadData(std::vector<float>& data, DataType type, double* x_max)
{
	char* filepath = NULL;
	nfdresult_t open_result = NFD_OpenDialog(NULL, NULL, &filepath);
	if (open_result == NFD_CANCEL)
	{
		return;
	} else if (open_result == NFD_ERROR)
	{
		LOG_ERROR("Open dialog failed: {}", NFD_GetError());
		return;
	}
	int result = ActuallyLoadData(&data, type, filepath, x_max, x_data.size() == 0);
	// If we succeeded, save the filepath in case we want to cache it later
	if (!result)
		filepaths[(int)type] = std::string(filepath);
	free(filepath);
}

/* Load in all the required textures */
void LoadTextures()
{
	int width, height, channels;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* data = stbi_load("assets/play.png", &width, &height, &channels, 0);
	play_button_texture_play = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/pause.png", &width, &height, &channels, 0);
	play_button_texture_pause = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/settings.png", &width, &height, &channels, 0);
	settings_button_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/XY_Axes.png", &width, &height, &channels, 0);
	xy_axes_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/XZ_Axes.png", &width, &height, &channels, 0);
	xz_axes_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/YZ_Axes.png", &width, &height, &channels, 0);
	yz_axes_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));

	stbi_set_flip_vertically_on_load(1);
	data = stbi_load("assets/white.png", &width, &height, &channels, 0);
	whale_texture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, bgfx::makeRef((void*)data, width * height * channels, [](void* data, void*) { stbi_image_free(data); }));
}

/* Create that menu bar at the top of the window */
bool create_menu_bar(bool running, bool* need_load_layout, bool* need_save_layout, bool* use_default_layout, double* x_max, double* x_min)
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::BeginMenu("Open"))
			{
				if (ImGui::MenuItem("Depth Data"))
				{
					LoadData(depth_data, DataType::DEPTH, x_max);
					*x_min = 0;
				}
				if (ImGui::MenuItem("Pitch Data"))
				{
					LoadData(pitch_data, DataType::PITCH, x_max);
					*x_min = 0;
				}
				if (ImGui::MenuItem("Roll Data"))
				{
					LoadData(roll_data, DataType::ROLL, x_max);
					*x_min = 0;
				}
				if (ImGui::MenuItem("Heading Data"))
				{
					LoadData(yaw_data, DataType::YAW, x_max);
					*x_min = 0;
				}
				if (ImGui::MenuItem("Data Cache"))
				{
					LoadDataCache(x_max);
					*x_min = 0;
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Save"))
			{
				if (ImGui::MenuItem("Data Cache"))
				{
					SaveDataCache();
				}
				if (ImGui::MenuItem("Export Data Section"))
				{
					ExportDataSection(mark_0, mark_1);
				}
				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Exit")) running = false;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			if (ImGui::MenuItem("Default Layout"))
			{
				*need_load_layout = true;
				*use_default_layout = true;
			}
			if (ImGui::MenuItem("Save Default Layout"))
			{
				*need_save_layout = true;
				*use_default_layout = true;
			}
			if (ImGui::MenuItem("Load Layout"))
			{
				*need_load_layout = true;
			}
			if (ImGui::MenuItem("Save Layout"))
			{
				*need_save_layout = true;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
	return running;
}

static double cursor_x;
static double cursor_y;

static double frame_x;
static double frame_y;
static double frame_width;
static double frame_height;

static bool changing_orientation = false;
static double camera_heading = 0.0;
static double camera_pitch = 0.0;

static ImGuiID viewport_viewport;

/* When the mouse button is pressed, check if the cursor is inside the 3D viewport, and if so, set the flag to rotate the camera */
void on_mouse_button_down_event(int button)
{
	auto io = ImGui::GetIO();
	if (button != 0)
	{
		return;
	}
	if (frame_x <= cursor_x && frame_x + frame_width >= cursor_x &&
		frame_y <= cursor_y && frame_y + frame_height >= cursor_y &&
		io.MouseHoveredViewport == viewport_viewport)
	{
		changing_orientation = true;
	}
}

/* Stop rotating the camera if the mouse button is released */
void on_mouse_button_up_event(int button)
{
	if (button == 0)
		changing_orientation = false;
}

void on_mouse_move_event(double x, double y)
{
	if (changing_orientation)
	{
		double dx = x - cursor_x;
		double dy = y - cursor_y;
		camera_heading += dx / frame_width * two_pi;
		camera_pitch += dy / frame_height * pi;
		// Keep the camera pitch in the correct range from straight up to straight down.
		if (camera_pitch > half_pi)
			camera_pitch = half_pi;
		else if (camera_pitch < -half_pi)
			camera_pitch = -half_pi;
		// Adjust the camera heading value so it remains in the range -pi to pi (Doesn't actually affect the rotation, but keeps the numbers accurate.
		if (camera_heading > pi)
			camera_heading -= two_pi;
		else if (camera_heading < -pi)
			camera_heading += two_pi;
	}

	cursor_x = x;
	cursor_y = y;
}

/* Program entry point */
int main(int argc, char** argv)
{
	Log::Init();

	if (mainWindow.Init(&WIDTH, &HEIGHT, true) == -1)
	{
		LOG_CRITICAL("Window init failed, terminating.");
		return -1;
	}
	LOG_INFO("Initialised window!");

	// Tell the graphics library about the window and initialisation parameters
	bgfx::PlatformData platform_data;
	platform_data.nwh = mainWindow.GetPlatformWindow();
	bgfx::setPlatformData(platform_data);

	bgfx::Init init_params;
	init_params.type = bgfx::RendererType::Count;
	init_params.resolution.width = WIDTH;
	init_params.resolution.height = HEIGHT;
	init_params.resolution.reset = BGFX_RESET_VSYNC;
	bgfx::init(init_params);

	LOG_INFO("Renderer: {0} via bgfx", bgfx::getRendererName(bgfx::getRendererType()));

	// Set up the layout of the vertices.
	bgfx::VertexLayout pos_tex_layout;
	pos_tex_layout.begin()
		.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.end();

	// Initialise vertex and index buffers to render the objects
	bgfx::VertexBufferHandle XZ_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_XZAxes, sizeof(s_XZAxes)), pos_tex_layout);
	bgfx::VertexBufferHandle XY_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_XYAxes, sizeof(s_XYAxes)), pos_tex_layout);
	bgfx::VertexBufferHandle YZ_vbuffer = bgfx::createVertexBuffer(bgfx::makeRef(s_YZAxes, sizeof(s_YZAxes)), pos_tex_layout);
	bgfx::IndexBufferHandle axes_ibuffer = bgfx::createIndexBuffer(bgfx::makeRef(s_AxesTris, sizeof(s_AxesTris)));

	bgfx::VertexBufferHandle vertex_buffer = bgfx::createVertexBuffer(bgfx::makeRef(pointerVertices, sizeof(pointerVertices)), pos_tex_layout);
	bgfx::IndexBufferHandle index_buffer = bgfx::createIndexBuffer(bgfx::makeRef(pointerTriList, sizeof(pointerTriList)));

	// Initialise the shader program
	bgfx::UniformHandle texture_uniform = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
	bgfx::ShaderHandle vs = bgfx::createShader(bgfx::makeRef(vs_texture(), vs_texture_len()));
	bgfx::ShaderHandle fs = bgfx::createShader(bgfx::makeRef(fs_texture(), fs_texture_len()));
	bgfx::ProgramHandle shader_program = bgfx::createProgram(vs, fs, true);

	// Initialise the GUI library
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

	ImPlot::CreateContext();

	imguiSetUserMousePosCallback((void(*)(void*, double, double))cursor_pos_callback);

	imguiInit(&mainWindow);
	imguiReset(WIDTH, HEIGHT);

	imguiInstallCallbacks();

	// Initialise variables used for the 3D viewport
	bgfx::FrameBufferHandle model_viewport = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle texture_handle = BGFX_INVALID_HANDLE;
	glm::vec2 model_viewport_size_cache = glm::vec2(0.0f, 0.0f);

	// Initialise clear colour and width/height
	reset(WIDTH, HEIGHT);

	LoadTextures();

	// Various variables used in the run loop
	u32 counter = 0;
	float time, lastTime = 0;
	float dt;
	float sum_dt = 0.0f;
	bool running = true;
	bool playing = false;
	bool dockspace_open = true;
	bool do_overwrite_default_settings_dialog = false;
	bool do_overwrite_default_settings = false;
	bool reset_height_zooms = false;
	float temporal_index = 0;
	float flow_rate = 1.0f;
	double graph_x_min = 0, graph_x_max = 0;
	float x1[2] = { 0, 0 };
	float x2[2] = { 0, 0 };
	const float ys[2] = { -100000000000000, 100000000000000 };
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, -5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), { 0.0f, 0.0f, 1.0f });
	glm::mat4 projection = glm::mat4(1.0f);
	glm::vec3 orientation = glm::vec3(0.0f);

	while (running)
	{
		// Clear the screen
		bgfx::touch(0);
		// Calculate the change in time since last frame
		time = (float)glfwGetTime();
		dt = time - lastTime;
		lastTime = time;
		if (mainWindow.Update())
		{
			running = false;
			break;
		}
		if (WIDTH == 0 || HEIGHT == 0)
			continue;
		// Tell the GUI library to process events and start a new frame
		imguiEvents(dt);
		ImGui::NewFrame();

		// Set up the main window to allow sub windows to dock into it
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace", &dockspace_open, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

		// We can't load or save a layout in the middle of a frame, so these flags are kept separate and set to complete at the end of the frame
		bool need_load_layout = false;
		bool need_save_layout = false;
		bool use_default_layout = false;
		running = create_menu_bar(running, &need_load_layout, &need_save_layout, &use_default_layout, &graph_x_max, &graph_x_min);

		if (show_err_msg)
		{
			ImGui::Begin("Error", &show_err_msg);
			ImGui::Text("%s", err_msg.c_str());
			if (ImGui::Button("Ok"))
				show_err_msg = false;
			ImGui::End();
		}

		ImGui::Begin("Timeline");
		{
			// Play button
			if (ImGui::ImageButton(IMGUI_TEXTURE_FROM_BGFX(playing ? play_button_texture_play : play_button_texture_pause), ImVec2(13.0f, 13.0f)))
			{
				playing = !playing;
			}
			if (playing)
			{
				sum_dt += dt;
				while (sum_dt > (1.f / (float)SAMPLES_PER_SECOND) / flow_rate)
				{
					temporal_index++;
					sum_dt -= (1.f / (float)SAMPLES_PER_SECOND) / flow_rate;
				}
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::SliderFloat("", &temporal_index, 0, x_data.size() - 1, "%.0f");

			if (temporal_index < 0)
				temporal_index = 0;
			else if (temporal_index >= x_data.size())
				temporal_index = x_data.size() - 1;

			ImGui::SetNextItemWidth(0.25 * ImGui::GetWindowWidth());
			ImGui::SliderFloat("Rate", &flow_rate, 0.25f, 10.0f, "Rate: %.3f");
			ImGui::SameLine(ImGui::GetWindowWidth() - 244);
			if (ImGui::Button("Set Mark 1"))
			{
				if (temporal_index > mark_1)
				{
					mark_1 = temporal_index;
					x2[0] = mark_1;
					x2[1] = mark_1;
				}
				mark_0 = temporal_index;
				x1[0] = mark_0;
				x1[1] = mark_0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Set Mark 2"))
			{
				if (temporal_index < mark_0)
				{
					mark_0 = temporal_index;
					x1[0] = mark_0;
					x1[1] = mark_0;
				}
				mark_1 = temporal_index;
				x2[0] = mark_1;
				x2[1] = mark_1;
			}
			ImGui::SameLine();
			if (ImGui::Button("Mark All"))
			{
				mark_0 = 0;
				mark_1 = x_data.size();
			}

			if (ImGui::Button("Reset playback rate"))
				flow_rate = 1.0f;
			ImGui::SameLine();
			if (ImGui::Button("Reset graph height zooms"))
				reset_height_zooms = true;
			ImGui::SameLine(ImGui::GetWindowWidth() - (yaw_data_use_velocity ? 142 : 170));
			if (ImGui::Button(yaw_data_use_velocity ? yaw_data_velocity_string : yaw_data_absolute_string))
			{
				yaw_data_use_velocity = !yaw_data_use_velocity;
				just_changed_use_yaw_velocity = true;
			}
		}
		ImGui::End();

		ImGui::Begin("Pitch Graph");
		{
			ImPlot::LinkNextPlotLimits(&graph_x_min, &graph_x_max, NULL, NULL);
			if (just_loaded_roll || reset_height_zooms)
				ImPlot::SetNextPlotLimits(graph_x_min, graph_x_max, -3, 3, ImGuiCond_Always);
			if (ImPlot::BeginPlot("Pitch Data", "Time", "Pitch"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				// Downsample technique borrowed from the ImPlot examples
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > pitch_data.size() - 1 ? pitch_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > pitch_data.size() - 1 ? pitch_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > pitch_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &pitch_data.data()[start], size, 0, sizeof(float) * downsample);
				// Show current location in time
				if (pitch_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &pitch_data[(int)temporal_index], 1);
				ImPlot::PlotLine("", &x1[0], &ys[0], 2);
				ImPlot::PlotLine("", &x2[0], &ys[0], 2);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("Roll Graph");
		{
			ImPlot::LinkNextPlotLimits(&graph_x_min, &graph_x_max, NULL, NULL);
			if (just_loaded_roll || reset_height_zooms)
				ImPlot::SetNextPlotLimits(graph_x_min, graph_x_max, -3, 3, ImGuiCond_Always);
			if (ImPlot::BeginPlot("Roll Data", "Time", "Roll"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				// Downsample technique borrowed from the ImPlot examples
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > roll_data.size() - 1 ? roll_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > roll_data.size() - 1 ? roll_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > roll_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &roll_data.data()[start], size, 0, sizeof(float) * downsample);
				// Show current location in time
				if (roll_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &roll_data[(int)temporal_index], 1);
				ImPlot::PlotLine("", &x1[0], &ys[0], 2);
				ImPlot::PlotLine("", &x2[0], &ys[0], 2);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("Heading Graph");
		{
			ImPlot::LinkNextPlotLimits(&graph_x_min, &graph_x_max, NULL, NULL);
			if (just_changed_use_yaw_velocity || reset_height_zooms || just_loaded_yaw)
			{
				just_changed_use_yaw_velocity = false;
				ImPlot::SetNextPlotLimits(graph_x_min, graph_x_max, yaw_data_use_velocity ? -0.5 : -3.5, yaw_data_use_velocity ? 0.5 : 3.5, ImGuiCond_Always);
			}
			if (ImPlot::BeginPlot("Heading Data", "Time", "Yaw"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				// Downsample technique borrowed from the ImPlot examples
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > yaw_data.size() - 1 ? yaw_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > yaw_data.size() - 1 ? yaw_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > yaw_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], yaw_data_use_velocity ? &yaw_velocity_data.data()[start] : &yaw_data.data()[start], size, 0, sizeof(float) * downsample);
				// Show current location in time
				if (yaw_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, yaw_data_use_velocity ? &yaw_velocity_data[(int)temporal_index] : &yaw_data[(int)temporal_index], 1);
				ImPlot::PlotLine("", &x1[0], &ys[0], 2);
				ImPlot::PlotLine("", &x2[0], &ys[0], 2);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("3D Viewport");
		{
			auto io = ImGui::GetIO();

			// Go forward or backward in time if the arrow keys are pressed down
			static u64 left_down_prev = 0;
			static u64 right_down_prev = 0;
			static double left_next = .6;
			static double right_next = .6;
			if (io.KeysDown[GLFW_KEY_LEFT] && !left_down_prev)
			{
				temporal_index -= 20;
			}
			if (io.KeysDown[GLFW_KEY_RIGHT] && !right_down_prev)
			{
				temporal_index += 20;
			}
			if (!io.KeysDown[GLFW_KEY_LEFT] && left_down_prev)
			{
				left_next = .6;
			}
			if (!io.KeysDown[GLFW_KEY_RIGHT] && right_down_prev)
			{
				right_next = .6;
			}
			if (io.KeysDownDuration[GLFW_KEY_LEFT] > left_next)
			{
				left_next += 0.1;
				// Speed up the longer the key is pressed down
				if (left_next > 3)
					temporal_index -= 30;
				if (left_next > 8)
					temporal_index -= 50;
				if (left_next > 12)
					temporal_index -= 150;
				temporal_index -= 20;
			}
			if (io.KeysDownDuration[GLFW_KEY_RIGHT] > right_next)
			{
				right_next += 0.1;
				// Speed up the longer the key is pressed down
				if (right_next > 3)
					temporal_index += 30;
				if (left_next > 8)
					temporal_index += 50;
				if (left_next > 12)
					temporal_index += 150;
				temporal_index += 20;
			}

			left_down_prev = io.KeysDown[GLFW_KEY_LEFT];
			right_down_prev = io.KeysDown[GLFW_KEY_RIGHT];

			if (temporal_index < 0)
				temporal_index = 0;
			else if (temporal_index >= x_data.size())
				temporal_index = x_data.size() - 1;

			// Save the current viewport ID to detect if the user clicks in the viewport
			viewport_viewport = ImGui::GetWindowViewport()->ID;
			ImVec2 available_space = ImGui::GetContentRegionAvail();
			frame_width = available_space.x;
			frame_height = available_space.y;
			ImVec2 loc = ImGui::GetCursorScreenPos();
			frame_x = loc.x - ImGui::GetWindowViewport()->Pos.x;
			frame_y = loc.y - ImGui::GetWindowViewport()->Pos.y;
			// If the available space has changed or the model viewport doesn't exist, recreate it with the new size
			if (!bgfx::isValid(model_viewport) || available_space.x != model_viewport_size_cache.x || available_space.y != model_viewport_size_cache.y)
			{
				model_viewport_size_cache.x = available_space.x;
				model_viewport_size_cache.y = available_space.y;
				LOG_INFO("Recreating framebuffer: {0}, {1}.", model_viewport_size_cache.x, model_viewport_size_cache.y);
				if (bgfx::isValid(model_viewport))
				{
					bgfx::destroy(model_viewport);
				}
				texture_handle = bgfx::createTexture2D(available_space.x, available_space.y, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_RT);
				model_viewport = bgfx::createFrameBuffer(1, &texture_handle);
				bgfx::setViewFrameBuffer(1, model_viewport);
				bgfx::setViewClear(1, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x3483ebFF);
				bgfx::setViewRect(1, 0, 0, available_space.x, available_space.y);

				// Set camera projection with the new dimensions
				projection = glm::perspective(glm::radians(60.0f), float(available_space.x) / float(available_space.y), 0.1f, 100.0f);
				bgfx::setViewTransform(1, &view[0][0], &projection[0][0]);
			}
			// Rotate the whale pointer and the axes according to the current camera position
			glm::mat4 rotation = glm::mat4(1.0);
			glm::mat4 axes_rotation = glm::mat4(1.0);
			rotation *= glm::yawPitchRoll(0.0f, (float)camera_pitch, 0.0f);
			rotation *= glm::yawPitchRoll(0.0f, 0.0f, (float)camera_heading);
			axes_rotation *= glm::yawPitchRoll(0.0f, (float)camera_pitch, 0.0f);
			axes_rotation *= glm::yawPitchRoll(0.0f, 0.0f, (float)camera_heading);
			// If the data is loaded, rotate the whale pointer to the current orientation
			if (yaw_data.size() != 0 && pitch_data.size() != 0 && roll_data.size() != 0)
				rotation *= glm::yawPitchRoll(roll_data[temporal_index], pitch_data[temporal_index], yaw_data[temporal_index]);

			// Decide if each axis should be rendered in front of or behind the pointer because I couldn't figure out how to make that work by default
			bool XZ_fore = camera_pitch < 0;
			bool YZ_fore = camera_heading > 0;
			bool XY_fore = (camera_heading < -1.63 || camera_heading > 1.5707963267948966) && !(camera_pitch < -1.4459012060099814 || camera_pitch > 1.4459012060099814);

			// Render axis function
			auto render_axis = [&](bgfx::VertexBufferHandle vbh, bgfx::TextureHandle th)
			{
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
				bgfx::setTransform(&axes_rotation[0][0]);
				bgfx::setVertexBuffer(1, vbh);
				bgfx::setIndexBuffer(axes_ibuffer);
				bgfx::setTexture(0, texture_uniform, th);
				bgfx::submit(1, shader_program);
			};

			// Render axes behind
			if (!XZ_fore)
				render_axis(XZ_vbuffer, xz_axes_texture);

			if (!XY_fore)
				render_axis(XY_vbuffer, xy_axes_texture);

			if (!YZ_fore)
				render_axis(YZ_vbuffer, yz_axes_texture);

			// Render pointer
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
			bgfx::setTransform(&rotation[0][0]);
			bgfx::setVertexBuffer(1, vertex_buffer);
			bgfx::setIndexBuffer(index_buffer);
			bgfx::setTexture(0, texture_uniform, whale_texture);
			bgfx::submit(1, shader_program);

			// Render axes in front
			if (YZ_fore)
				render_axis(YZ_vbuffer, yz_axes_texture);

			if (XY_fore)
				render_axis(XY_vbuffer, xy_axes_texture);

			if (XZ_fore)
				render_axis(XZ_vbuffer, xz_axes_texture);

			// Render the viewport to the screen
			ImGui::ImageButton(IMGUI_TEXTURE_FROM_BGFX(texture_handle), available_space, ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1));
		}
		ImGui::End();

		ImGui::Begin("Depth Graph");
		{
			ImPlot::LinkNextPlotLimits(&graph_x_min, &graph_x_max, NULL, NULL);
			if (just_loaded_depth || reset_height_zooms)
				ImPlot::SetNextPlotLimits(graph_x_min, graph_x_max, -1500, 200, ImGuiCond_Always);
			if (ImPlot::BeginPlot("Depth Data", "Time", "Depth"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				// Downsample technique borrowed from the ImPlot examples
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > depth_data.size() - 1 ? depth_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > depth_data.size() - 1 ? depth_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > depth_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &depth_data.data()[start], size, 0, sizeof(float) * downsample);
				// Show current location in time
				if (depth_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &depth_data[(int)temporal_index], 1);

				ImPlot::PlotLine("", &x1[0], &ys[0], 2);
				ImPlot::PlotLine("", &x2[0], &ys[0], 2);

				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("Depth Velocity Graph");
		{
			ImPlot::LinkNextPlotLimits(&graph_x_min, &graph_x_max, NULL, NULL);
			if (just_loaded_depth || reset_height_zooms)
				ImPlot::SetNextPlotLimits(graph_x_min, graph_x_max, -5, 5, ImGuiCond_Always);
			if (ImPlot::BeginPlot("Depth Velocity Data", "Time", "Velocity"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				// Downsample technique borrowed from the ImPlot examples
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > depth_velocity_data.size() - 1 ? depth_velocity_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > depth_velocity_data.size() - 1 ? depth_velocity_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > depth_velocity_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &depth_velocity_data.data()[start], size, 0, sizeof(float) * downsample);
				// Show current location in time
				if (depth_velocity_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &depth_velocity_data[(int)temporal_index], 1);
				ImPlot::PlotLine("", &x1[0], &ys[0], 2);
				ImPlot::PlotLine("", &x2[0], &ys[0], 2);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("Depth Acceleration Graph");
		{
			ImPlot::LinkNextPlotLimits(&graph_x_min, &graph_x_max, NULL, NULL);
			if (just_loaded_depth || reset_height_zooms)
				ImPlot::SetNextPlotLimits(graph_x_min, graph_x_max, -0.25, 0.25, ImGuiCond_Always);
			if (ImPlot::BeginPlot("Depth Acceleration Data", "Time", "Acceleration"))
			{
				ImPlot::SetLegendLocation(ImPlotLocation_East, 1, true);
				// Downsample technique borrowed from the ImPlot examples
				int downsample = (int)ImPlot::GetPlotLimits().X.Size() / 1000 + 1;
				int start = (int)ImPlot::GetPlotLimits().X.Min;
				start = start < 0 ? 0 : (start > depth_acceleration_data.size() - 1 ? depth_acceleration_data.size() - 1 : start);
				int end = (int)ImPlot::GetPlotLimits().X.Max + 1000;
				end = end < 0 ? 0 : end > depth_acceleration_data.size() - 1 ? depth_acceleration_data.size() - 1 : end;
				int size = (end - start) / downsample;
				while (size * downsample > depth_acceleration_data.size())
				{
					size--;
				}
				ImPlot::PlotLine("Data", &x_data.data()[start], &depth_acceleration_data.data()[start], size, 0, sizeof(float) * downsample);
				// Show current location in time
				if (depth_acceleration_data.size() != 0)
					ImPlot::PlotScatter("Current", &temporal_index, &depth_acceleration_data[(int)temporal_index], 1);
				ImPlot::PlotLine("", &x1[0], &ys[0], 2);
				ImPlot::PlotLine("", &x2[0], &ys[0], 2);
				ImPlot::EndPlot();
			}
		}
		ImGui::End();

		ImGui::Begin("Stats");
		{
			ImGui::Text("Depth: %+f, Velocity: %+f, Acceleration: %+f", 
				depth_data.size() ? depth_data[temporal_index] : 0, 
				depth_data.size() ? depth_velocity_data[temporal_index] : 0, 
				depth_data.size() ? depth_acceleration_data[temporal_index] : 0);

			ImGui::Text("Pitch: %+f", pitch_data.size() ? pitch_data[temporal_index] : 0);
			ImGui::Text("Roll: %+f", roll_data.size() ? roll_data[temporal_index] : 0);
			ImGui::Text("Heading: %+f, Turn rate: %+f", 
				yaw_data.size() ? yaw_data[temporal_index] : 0,
				yaw_velocity_data.size() ? yaw_velocity_data[temporal_index] : 0);
		}
		ImGui::End();

		ImGui::End();

		// If the user selected to overwrite the default layout, ask if they are sure they want to do this
		if (do_overwrite_default_settings_dialog)
		{
			ImGui::Begin("Overwrite default layout?", &do_overwrite_default_settings_dialog);
			{
				ImGui::Text("Are you sure you want to overwrite the default layout?");
				ImGui::Text("This is NOT recommended!");
				if (ImGui::Button("Confirm"))
				{
					do_overwrite_default_settings = true;
					do_overwrite_default_settings_dialog = false;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					do_overwrite_default_settings_dialog = false;
				}
			}
			ImGui::End();
		}

		// Finish the frame for both the GUI library and graphics library
		ImGui::Render();
		imguiRender(ImGui::GetDrawData(), 200);
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		bgfx::frame();
		counter++;

		// Check if the user asked to save or load layouts
		std::string layout_path = "";
		if (use_default_layout)
		{
			layout_path = "assets/default_layout.ini";
		} else if (need_load_layout || need_save_layout)
		{
			char* filepath = NULL;
			nfdresult_t open_result;
			if (need_load_layout)
				open_result = NFD_OpenDialog(NULL, NULL, &filepath);
			else
				open_result = NFD_SaveDialog(NULL, NULL, &filepath);
			if (open_result == NFD_CANCEL)
			{
				need_load_layout = need_save_layout = false;
			} else if (open_result == NFD_ERROR)
			{
				need_load_layout = need_save_layout = false;
				LOG_ERROR("Open dialog failed: {}", NFD_GetError());
			} else
			{
				layout_path = std::string(filepath);
				if (need_save_layout && layout_path.substr(layout_path.size() - 26, 26) == std::string("\\assets\\default_layout.ini"))
				{
					do_overwrite_default_settings_dialog = true;
				}
			}
		}
		if (need_load_layout)
		{
			// We are not allowed to load these while a frame is in progress, so we wait until afer ImGui::Render
			ImGui::LoadIniSettingsFromDisk(layout_path.c_str());
		}
		if (need_save_layout && !use_default_layout)
		{
			ImGui::SaveIniSettingsToDisk(layout_path.c_str());
		} else if (need_save_layout && use_default_layout)
		{
			do_overwrite_default_settings_dialog = true;
		}
		if (do_overwrite_default_settings)
		{
			layout_path = "assets/default_layout.ini";
			ImGui::SaveIniSettingsToDisk(layout_path.c_str());
			do_overwrite_default_settings = false;
		}

		// Decrement just loaded flags
		if (just_loaded_depth)
			just_loaded_depth--;
		if (just_loaded_pitch)
			just_loaded_pitch--;
		if (just_loaded_roll)
			just_loaded_roll--;
		if (just_loaded_yaw)
			just_loaded_yaw--;

		reset_height_zooms = false;
	}
	LOG_INFO("Shutting down.");

	// Shut down the GUI library and graphics library before exiting.
	imguiShutdown();
	bgfx::shutdown();

	return 0;
}
