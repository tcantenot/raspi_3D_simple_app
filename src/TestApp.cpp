#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include <glm/gtx/transform.hpp>


#include <TestApp.hpp>
#include <Cube.hpp>
#include <PerspectiveCamera.hpp>
#include <Input.hpp>
#include <Terrain.hpp>
#include <Scheduler.hpp> 

#include <cstdlib>
#include <ctime>

namespace {
    float frandom(float a, float b){
        return (std::rand() / static_cast<float>(RAND_MAX)) * (b - a) + a;
    }

    int s_lag = 6;
    void doLag()
    {
        for(auto i = 0; i < s_lag * 100000; ++i);
    }
}

namespace RPi {

TestApp::TestApp(Window & window, int argc, char ** argv, int lag): App(window, argc, argv)
{
    s_lag = lag;
}
        
TestApp::~TestApp()
{

}

void TestApp::run()
{
    std::srand(time(0));

    float timeFromStart = 0.f;
    float timeFromStartTmp = 0.f;
    float totalTime = 0.f;
    std::size_t nbFrames = 0;
    auto quitting = false;

    Cube cube(5);

    std::vector<Cube> cubes;

    for(int i = 0; i < 150; ++i)
    {
        cubes.emplace_back(i);                        
        auto & c = cubes[i];
        c.translate(glm::vec3(i, 0, i));
    }

    Terrain terrain(50, 50);

    //Terrain terrain2(500, 500);

    auto t1 = std::chrono::high_resolution_clock::now();

    PerspectiveCamera camera(70.f, 
        static_cast<float>(m_window.getWidth()) / static_cast<float>(m_window.getHeight()),
        1.f, 100.f);

    camera.position(glm::vec3(21, 11, 20));
    camera.target(glm::vec3(13, 1, 11));
    camera.up(glm::vec3(0, 1, 0));

    camera.enableMouse(false);

    Input input;

    m_window.showMousePointer(false);
    m_window.grabMousePointer(true);

    glm::mat4 projection;
    glm::mat4 modelview;

    assert(m_window.getWidth() != 0);
    assert(m_window.getHeight() != 0);

    projection = glm::perspective(70.0, (double) m_window.getWidth() / m_window.getHeight(), 1.0, 100.0);
    modelview  = glm::mat4(1.0);

    static float const PI2 = 2 * M_PI;
    static float const timeWaveStep = M_PI / 16;
    float timeWave = -PI2;
    float random = 1.f;

    while(!m_window.userInterrupt() && !quitting)
    {
        // Reset timer
        auto t2 = std::chrono::high_resolution_clock::now();
        auto deltaTime = static_cast<float>(std::chrono::duration_cast<
            std::chrono::microseconds>(t2 - t1).count()) / 1000.f;

        t1 = t2;

        // Get the events
        input.updateEvents();

        // Move camera
        camera.move(input);

        // Quit the app with escape
        if(input.isKeyPressed(SDLK_ESCAPE))
        {
            std::cout << "Escape key pressed" << std::endl;
            break;
        }

        doLag();

        // Clear the screen
        m_window.clear();

        modelview = camera.lookAt();

        projection = camera.projection();

        if(timeFromStart - timeFromStartTmp > 100.f)
        {
            if(timeWave > PI2)
            {
                timeWave = -PI2;
                random = frandom(1.f, 2.f);
                std::cout << random << std::endl;
            }

            timeWave += timeWaveStep;

            if(std::fabs(timeWave) < std::numeric_limits<float>::epsilon())
            {
                timeWave = 0.f;
            }

            //std::cout << timeWave << std::endl;

            timeFromStartTmp = timeFromStart;

            m_window.getContext().program->sendFloat("time", timeWave);
            m_window.getContext().program->sendFloat("random", random);
        }

        // Render the cube
        //cube.render(*m_window.getContext().program, projection, modelview);

        //for(auto & c : cubes)
        //{
            //c.rotate(frandom(0.f, 180.f), glm::vec3(1.0, 1.0, 0));
            //c.render(*m_window.getContext().program, projection, modelview);
        //}

        //glViewport(0, 0, m_window.getWidth() / 2, m_window.getHeight());
        terrain.render(*m_window.getContext().program, projection, modelview);
        //glViewport(m_window.getWidth() / 2, 0, m_window.getWidth() / 2, m_window.getHeight());
        //terrain2.render(*m_window.getContext().program, projection, modelview);

        // Refresh the window
        m_window.display();

        // Get FPS
        totalTime += deltaTime;
        timeFromStart += deltaTime;
        ++nbFrames;

        if(totalTime > 1000.0f)
        {
            auto fps = (static_cast<float>(nbFrames) / (totalTime / 1000.f));

            std::cout << nbFrames << " frames rendered in " << totalTime 
                      << " ms -> FPS = " 
                      << fps << std::endl;

            //std::cout << "Pos : (" << camera.position().x << ", " << camera.position().y << ", " << camera.position().z << ")" << std::endl;
            //std::cout << "Target : (" << camera.target().x << ", " << camera.target().y << ", " << camera.target().z << ")" << std::endl;
            
            std::ostringstream fpsText;
            fpsText << "Sched : " << Scheduler::GetSchedulerName(getpid())
                    << "; Priority : " << Scheduler::GetPriority(getpid())
                    << " => " << fps << " FPS";

            m_window.displayText(fpsText.str());

            totalTime -= 1000.0f;
            nbFrames = 0;
        }
    }

    std::cout << "END OF LOOP" << std::endl;
}

}
