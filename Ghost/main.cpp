// dear imgui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define NOMINMAX
#include <stdio.h>
#include "Audio.h"
#include "fft.h"
#include <string>

#include <windows.h>

// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>    // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING)
#define GLFW_INCLUDE_NONE         // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>  // Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

std::string openReadFile() {
	OPENFILENAME    ofn;
	char            filename[256];

	filename[0] = '\0';  //忘れるとデフォルトファイル名に変な文字列が表示される
	memset(&ofn, 0, sizeof(OPENFILENAME));  //構造体を0でクリア
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFilter = "mp3(*.mp3)\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "mp3";

	if (GetOpenFileName(&ofn) != TRUE)
		return "";
	return filename;
}

bool CheckShader(GLuint handle, const char* desc)
{
	GLint status = 0, log_length = 0;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
	if ((GLboolean)status == GL_FALSE)
		fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to compile %s!\n", desc);
	if (log_length > 1)
	{
		ImVector<char> buf;
		buf.resize((int)(log_length + 1));
		glGetShaderInfoLog(handle, log_length, NULL, (GLchar*)buf.begin());
		fprintf(stderr, "%s\n", buf.begin());
	}
	return (GLboolean)status == GL_TRUE;
}

bool CheckProgram(GLuint handle, const char* desc)
{
	GLint status = 0, log_length = 0;
	glGetProgramiv(handle, GL_LINK_STATUS, &status);
	glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
	if ((GLboolean)status == GL_FALSE)
		fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to link %s! (with GLSL '%s')\n", desc, "g_GlslVersionString");
	if (log_length > 1)
	{
		ImVector<char> buf;
		buf.resize((int)(log_length + 1));
		glGetProgramInfoLog(handle, log_length, NULL, (GLchar*)buf.begin());
		fprintf(stderr, "%s\n", buf.begin());
	}
	return (GLboolean)status == GL_TRUE;
}

auto mp3 = new MP3Audio();
//auto mp3 = new SinAudio();
//auto mp3 = new NokogiriAudio();
auto player = new PCMAudioPlayer();

int main(int, char**)
{
	// Setup window
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return 1;

	// Decide GL+GLSL versions
#if __APPLE__
	// GL 3.2 + GLSL 150
	const char* glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 410";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

	// Create window with graphics context
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
	if (window == NULL)
		return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
	bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
	bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
	bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING)
	bool err = false;
	glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)glfwGetProcAddress(name); });
#else
	bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
	if (err)
	{
		fprintf(stderr, "Failed to initialize OpenGL loader!\n");
		return 1;
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'docs/FONTS.txt' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);

	const GLchar* vertex_shader =
		"#version 410\n"
		"layout (location = 0) in vec2 Position;\n"
		"layout (location = 1) in vec2 UV;\n"
		"layout (location = 2) in vec4 Color;\n"
		"uniform mat4 ProjMtx;\n"
		"out vec2 Frag_UV;\n"
		"out vec4 Frag_Color;\n"
		"void main()\n"
		"{\n"
		"    Frag_UV = UV;\n"
		"    Frag_Color = Color;\n"
		"    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
		"}\n";
	const GLchar* fragment_shader =
		"#version 410\n"
		"in vec2 Frag_UV;\n"
		"in vec4 Frag_Color;\n"
		"uniform sampler2D Texture;\n"
		"layout (location = 0) out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
		"}\n";

	GLuint       g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
	// Shader Compile Test
	// Create shaders
	const GLchar* vertex_shader_with_version[1] = { vertex_shader };
	g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(g_VertHandle, 1, vertex_shader_with_version, NULL);
	glCompileShader(g_VertHandle);
	CheckShader(g_VertHandle, "vertex shader");

	const GLchar* fragment_shader_with_version[1] = { fragment_shader };
	g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(g_FragHandle, 1, fragment_shader_with_version, NULL);
	glCompileShader(g_FragHandle);
	CheckShader(g_FragHandle, "fragment shader");

	g_ShaderHandle = glCreateProgram();
	glAttachShader(g_ShaderHandle, g_VertHandle);
	glAttachShader(g_ShaderHandle, g_FragHandle);
	glLinkProgram(g_ShaderHandle);
	CheckProgram(g_ShaderHandle, "shader program");

	if (g_ShaderHandle && g_VertHandle) { glDetachShader(g_ShaderHandle, g_VertHandle); }
	if (g_ShaderHandle && g_FragHandle) { glDetachShader(g_ShaderHandle, g_FragHandle); }
	if (g_VertHandle) { glDeleteShader(g_VertHandle); g_VertHandle = 0; }
	if (g_FragHandle) { glDeleteShader(g_FragHandle); g_FragHandle = 0; }
	if (g_ShaderHandle) { glDeleteProgram(g_ShaderHandle); g_ShaderHandle = 0; }
	/////////

	// Our state
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Main loop
	while (!glfwWindowShouldClose(window))
	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Analizer");                          // Create a window called "Hello, world!" and append into it.


			const int plotWaveNum = 256;
			const int plotFFTNum = 512;
			static float values[plotWaveNum] = {};
			static fft::FftArray z(plotFFTNum);
			static float freqValues[plotFFTNum];
			if (mp3->IsValid()) {
				int postion = player->GetPosition();
				int lastIndex = mp3->GetSamples() / mp3->GetChannels() - 1;
				for (int i = 0; i < plotWaveNum; i++) {
					int off = std::min(postion, lastIndex - plotWaveNum);
					int index = std::min(i + off, lastIndex);
					switch (mp3->GetChannels())
					{
					case 1:
						values[i] = mp3->GetBuffer()[index];
						break;
					case 2:
						values[i] = (mp3->GetBuffer()[index * 2] + mp3->GetBuffer()[index * 2 + 1]) * 0.5f;
						break;
					default:
						break;
					}
				}

				for (int i = 0; i < plotFFTNum; i++) {
					int off = std::min(postion, lastIndex - plotFFTNum);
					int index = std::min(i + off, lastIndex);
					switch (mp3->GetChannels())
					{
					case 1:
						z[i] = mp3->GetBuffer()[index];
						break;
					case 2:
						z[i] = ((double)mp3->GetBuffer()[index * 2] + (double)mp3->GetBuffer()[index * 2 + 1]) * 0.5;
						break;
					default:
						break;
					}
					// Hann Window
					z[i] *= 0.5 - 0.5 * cos(2.0 * 3.14159265358979323846 * i / plotFFTNum);
					// ハン窓で面積が1/2になったので二倍する
					z[i] *= 2.0;
				}
				z.fft();
				int count = 0;
				for (fft::FftArray::iterator it = z.begin(); it != z.end(); ++it) {
					float re = (*it).real(), im = (*it).imag();
					//freqValues[count] = 10.0f * log10(re*re+im*im) + 20.0f;

					// パワースペクトル（片側スペクトルなので二倍する）
					//freqValues[count] = 2.0 * ((re * re + im * im) / (double)(plotFFTNum * plotFFTNum));
					//freqValues[count] = 10.0 * log10(2.0 * ((re * re + im * im) / (double)(plotFFTNum * plotFFTNum)));  // デシベル

					// 振幅（片側スペクトルなので二倍する）
					//freqValues[count] = 2.0 * (sqrt(re * re + im * im) / (double)(plotFFTNum));
					freqValues[count] = 20.0 * log10(2.0 * (sqrt(re * re + im * im) / (double)(plotFFTNum)));			// デシベル


					//freqValues[count] = sqrt(re * re + im * im) / (double)plotFFTNum;
					count++;
				}
			}
			ImGui::PlotLines("Wave", values, IM_ARRAYSIZE(values), 0, "", -1.0f, 1.0f, ImVec2(0, 160));
			ImGui::PlotHistogram("Frequency", freqValues, IM_ARRAYSIZE(freqValues)/2, 0, "-52dB ~ 1dB", -52.0f, 1.0f, ImVec2(0, 160));
			//ImGui::PlotHistogram("Frequency", freqValues, IM_ARRAYSIZE(freqValues) / 2, 0, "-60dB ~ 1dB", 0.0, 1.0f, ImVec2(0, 160));

			static bool loop = false;
			ImGui::Checkbox("Loop", &loop);
			player->SetLoop(loop);

			ImGui::SameLine();
			if (ImGui::Button("Load"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			{
				counter++;
				auto filename = openReadFile();
				if (filename != "") {
					
					//mp3->Create(220.0f);
					mp3->LoadFromFile(filename);
					player->SetAudio(*mp3);
					player->Start();
					//SaveAudioToWaveFile(*mp3, "test.wav");
				}
			}


			ImGui::SameLine();
			if (ImGui::Button("Start")) {
				player->Start();
				player->Restart();
			}
			ImGui::SameLine();
			if (ImGui::Button("Pause")) {
				player->Pause();
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop")) {
				player->Stop();
			}

			ImGui::SameLine();
			ImGui::Text("samples = %d", player->GetPosition());

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (show_another_window)
		{
			ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				show_another_window = false;
			ImGui::End();
		}

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
