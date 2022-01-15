#include <chrono>
#include <iostream>
#include <graphic/graphic.hpp>
#include <imgui_impl_sdl.h>
#include <vector>
#include <complex>
#include <mpi.h>
#include <cstring>

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


int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank_num;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &rank_num);

    if (rank==0){
        graphic::GraphicContext context{"Assignment 2"};
        Square canvas(100);
        size_t duration = 0;
        size_t pixels = 0;

        context.run([&](graphic::GraphicContext *context [[maybe_unused]], SDL_Window *) {//loop loop loop
            {
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
                static ImVec4 col = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
                static int k_value = 100;
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

                    // send msg about variables to other proc, maintain the same value of variables for all procs
                    int *msg_send = new int[5];
                    msg_send[0] = size;
                    msg_send[1] = scale;
                    msg_send[2] = center_x;
                    msg_send[3] = center_y;
                    msg_send[4] = k_value;

                    for (int k=1; k<rank_num; k++){
                        MPI_Send(msg_send, 5, MPI_INT, k, 888888, MPI_COMM_WORLD);
                    }

                    //variable for recv data in proc 1
                    int *recv_buffer_big = new int[size*(rank_num-1)];////////put before loop test
                    int *go_on_flag_send = new int[1];
                    go_on_flag_send[0]=0;

                    //variables regarding iter nums
                    int iter_complete = size / rank_num;
                    int iter_offset = size - iter_complete * rank_num;
                    int iter_total = 0;
                    iter_total = iter_complete;

                    // resize canvas before calculation in all procs
                    canvas.resize(size);

                    auto begin = std::chrono::high_resolution_clock::now();
                    for (int iter = 0; iter < iter_total; iter++) {//complete iter rows of calculation, notice that the row is not adjacent.
                        double cx = static_cast<double>(size) / 2 + center_x;
                        double cy = static_cast<double>(size) / 2 + center_y;
                        double zoom_factor = static_cast<double>(size) / 4 * scale;

                        //complete several rows of calculation
                        int i = iter * rank_num + rank;
                        for (int j = 0; j < size; ++j) {
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

                        // deal with the offset (remainder) rows in the last iter for robust programming
                        if (iter == iter_total-1 && iter_offset>0){ // only possibly be activated in the last iter
                            for (int i=size - iter_offset; i<size; i++){
                                for (int j = 0; j < size; ++j) {
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
                            }
                        }

                        // recv data from all other procs, put them in canvas.
                        for (int k=1; k<rank_num; k++){
                            MPI_Recv(recv_buffer_big + (k-1) * size, size, MPI_INT, k, iter, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                            for (int j = 0; j < size; j++) {
                                canvas[{k + iter * rank_num, j}] = recv_buffer_big[(k-1) * size+j];
                            }
                        }
                    }

                    //send signals to other procs to let them continue calculations
                    go_on_flag_send[0]=1;
                    for (int k=1; k<rank_num; k++){
                        MPI_Send(go_on_flag_send, 1, MPI_INT, k, 666666, MPI_COMM_WORLD);
                    }

                        //[DEBUG] print canvas
//                        std::cout<<"rank: "<<rank<<", canvas:"<<std::endl;
//                        for (int i=0; i<size; i++){
//                            std::cout<<"row: "<<i<<": ";
//                            for (int j=0; j<size;j++){
//                                std::cout<<canvas[{i, j}]<<" ";
//                            }
//                            std::cout<<std::endl;
//                        }

                    {
                        auto end = std::chrono::high_resolution_clock::now();
                        pixels = size;
                        duration = duration_cast<std::chrono::nanoseconds>(end - begin).count();
                        if (duration > SHOW_THRESHOLD) {
                            std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
                            auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
                            std::cout << "speed: " << speed << " pixels per second" << std::endl;
                            pixels = 0;
                            duration = 0;
                        }
                        //[DEBUG] print small data set for self test
                        else {
                            std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
                            auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
                            std::cout << "speed: " << speed << " pixels per second" << std::endl;
                            pixels = 0;
                            duration = 0;
                        }
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

    }
    else if(rank!=0){
        // recv msg about variables to other proc, maintain the same value of variables for all procs
        int *msg_recv = new int[5];

        while (true){
            {
                MPI_Recv(msg_recv, 5, MPI_INT, 0, 888888, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                int size = msg_recv[0];
                int scale = msg_recv[1];
                int center_x = msg_recv[2];
                int center_y = msg_recv[3];
                int k_value = msg_recv[4];

                int iter_complete = size / rank_num;
                int iter_total = 0;
                iter_total = iter_complete;

                int *data_send = new int[size];
                int *go_on_flag_recv = new int[1];

                for (int iter = 0; iter < iter_total; iter++) {//complete iter rows of calculation, notice that the row is not adjacent.
                    double cx = static_cast<double>(size) / 2 + center_x;
                    double cy = static_cast<double>(size) / 2 + center_y;
                    double zoom_factor = static_cast<double>(size) / 4 * scale;

                    int i = iter * rank_num + rank;
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
                        data_send[j] = k;
                    }

                    MPI_Send(data_send, size, MPI_INT, 0, iter, MPI_COMM_WORLD);

                }
                // hang the program if the calculation in p0 has not finished yet.
                MPI_Recv(go_on_flag_recv, 1, MPI_INT, 0, 666666, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
    }

    MPI_Finalize();
    return 0;
}