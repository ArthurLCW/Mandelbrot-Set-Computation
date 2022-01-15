#include <chrono>
#include <iostream>
#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <vector>
#include <complex>
#include <cstring>
#include <thread>
#include <mutex>
#include <future>

struct Square {
    std::vector<int> buffer;
    size_t length;

    explicit Square(size_t length) : buffer(length), length(length * length) {}

    void resize(size_t new_length) {
        buffer.assign(new_length * new_length, false);
        length = new_length;
    }

    auto& operator[](std::pair<size_t, size_t> pos) {
        return buffer[pos.second * length + pos.first];
    }
};


static constexpr float MARGIN = 4.0f;
static constexpr float BASE_SPACING = 2000.0f;
static constexpr size_t SHOW_THRESHOLD = 500000000ULL;
static int global_row_idx=-1;
std::mutex lock_mutex;
static int num_threads=4; //default number of threads
int *occupied_threads = new int[num_threads];
int *new_threads = new int[num_threads];


void calculate_thread(int* occupied_threads, int idx, Square& canvas, int size, int center_x, int center_y, int scale, int k_value){
    // get the current available row idx for thread to calculate.
    // use mutex to issue the atomic operation.
    int local_row_idx=0;
    lock_mutex.lock();
    global_row_idx++;
    if (global_row_idx>=size){
        global_row_idx=0;
    }
    local_row_idx = global_row_idx;
    lock_mutex.unlock();

    double cx = static_cast<double>(size) / 2 + center_x;
    double cy = static_cast<double>(size) / 2 + center_y;
    double zoom_factor = static_cast<double>(size) / 4 * scale;

    int i = local_row_idx;
    for (int j = 0; j < size; ++j) { //complete 1 row of calculation
        double x = (static_cast<double>(j) - cx) / zoom_factor;
        double y = (static_cast<double>(i) - cy) / zoom_factor;
        std::complex<double> z{0, 0};
        std::complex<double> c{x, y};
        int k = 0;
        do {
            z = z * z + c;
            k++;
        } while (norm(z) < 2.0 && k < k_value);
        canvas[{i, j}] = k;
    }

    occupied_threads[idx] = 0;
//    std::string str="thread idx: "+std::to_string(idx)+", current_row: "+std::to_string(local_row_idx)+"\n";
//    std::cout<<str;
}


int main(int argc, char **argv) {
    // get the number of threads set by the user
    if (argc > 2) {
        std::cerr << "wrong arguments. please input only one argument as the number of threads" << std::endl;
        return 0;
    }else if (argc == 2){
        num_threads = std::stoi(argv[1]);
    }

    graphic::GraphicContext context{"Assignment 2"};
    Square canvas(100);

    context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *) {
        {
            size_t duration = 0;
            size_t pixels = 0;

            for (int i=0; i<num_threads;i++){
                occupied_threads[i]=0;
            }
            for (int i=0; i<num_threads;i++){
                new_threads[i]=0;
            }

            auto io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Assignment 2", nullptr,
                         ImGuiWindowFlags_NoMove
                         | ImGuiWindowFlags_NoCollapse
                         | ImGuiWindowFlags_NoTitleBar
                         | ImGuiWindowFlags_NoResize);
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            static int center_x = 0;
            static int center_y = 0;
            static int size = 800;
            static int scale = 1;
            static int k_value = 100;
            static ImVec4 col = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);

            ImGui::DragInt("Center X", &center_x, 1, -4 * size, 4 * size, "%d");
            ImGui::DragInt("Center Y", &center_y, 1, -4 * size, 4 * size, "%d");
            ImGui::DragInt("Fineness", &size, 10, 100, 1000, "%d");
            ImGui::DragInt("Scale", &scale, 1, 1, 100, "%.01f");
            ImGui::DragInt("K", &k_value, 1, 100, 1000, "%d");
            ImGui::ColorEdit4("Color", &col.x);
            {
                using namespace std::chrono;
                auto spacing = BASE_SPACING / static_cast<float>(size);
                auto radius = spacing / 2;
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const ImU32 col32 = ImColor(col);
                float x = p.x + MARGIN, y = p.y + MARGIN;
                canvas.resize(size);

                int iter_complete = size / num_threads;
                int iter_offset = size - iter_complete * num_threads;
                int iter_total = 0;
                if (iter_offset > 0) iter_total = iter_complete + 1;
                else iter_total = iter_complete;


                // declare threads array
                //std::thread thread_arr[num_threads];
                std::vector<std::thread> thread_arr(num_threads);

                // assign threads
                auto begin = high_resolution_clock::now();

                int already_iter = 0;
                while (already_iter<size){
                    for (int i=0; i<num_threads; i++){
                        if (occupied_threads[i]==0){
                            already_iter++;
                            if (new_threads[i]==1){
                                thread_arr[i].join();
                            }
                            occupied_threads[i]=1;
                            new_threads[i] = 1;
                            thread_arr[i]=std::thread(calculate_thread, occupied_threads, i, std::ref(canvas),size,center_x,center_y,scale,k_value);
                            break;
                        }
                    }
                }

                // before exiting the program, ensures that all threads has joined.
                for (int i=0; i<num_threads; i++){
                    if (thread_arr[i].joinable()){
                        thread_arr[i].join();
                    }
                }



                //[debug]: show results
//                std::cout<<"result::"<<std::endl;
//                for (int i=0;i<size;i++){
//                    std::cout<<"row: "<<i<<std::endl;
//                    for (int j=0; j<size; j++){
//                       std::cout<<canvas[{i,j}]<<" ";
//                 }
//                 std::cout<<std::endl;
//                }
//                std::cout<<std::endl;

                auto end = high_resolution_clock::now();
                pixels += size;
                duration += duration_cast<nanoseconds>(end - begin).count();
                if (duration > SHOW_THRESHOLD) {
                    std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
                    auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
                    std::cout << "speed: " << speed << " pixels per second" << std::endl;
                    pixels = 0;
                    duration = 0;
                }
                //[DEBUG] see result of small data set for test use
                else {
                    std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
                    auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
                    std::cout << "speed: " << speed << " pixels per second" << std::endl;
                    pixels = 0;
                    duration = 0;
                }

                for (int i = 0; i < size; ++i) {
                    for (int j = 0; j < size; ++j) {
                        if (canvas[{i, j}] == k_value) {
                            draw_list->AddCircleFilled(ImVec2(x, y), radius, col32);
                        }
                        x += spacing;
                    }
                    y += spacing;
                    x = p.x + MARGIN;
                }
            }
            ImGui::End();
        }
    });
    return 0;
}