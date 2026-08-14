#include <SFML/Graphics.hpp>
#include <iostream>
#include <array>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>


class Sim {
private:
    static constexpr bool GENERATE_PNGS = true;
    static constexpr bool RENDER_ON_WINDOW = false;
    static constexpr int N = 1000000;
    static constexpr int WIDTH = 1920, HEIGHT = 1080;
    static constexpr float G = 0.04f;
    static constexpr float PARTICLE_RADIUS = 10.f;
    static constexpr int MAX_PARTICLE_IN_LEAF = 64;
    static constexpr float MIN_LEAF_WIDTH = 0.001f;
    static constexpr float THETA = 1.f; //if THETA != 1.f you need to change containsParticle computation in computeAcc()
    static constexpr float THETA_SQUARED = THETA * THETA;
    static constexpr sf::Vector2f MAX_BOUNDARIES = {3840.f, 2160.f};
    static constexpr sf::Vector2f MIN_BOUNDARIES = {-3840.f, -2160.f};

    int boxesVisited, approximations, directInteractions;
    struct Box {
        sf::Vector2f max, min;
        sf::Vector2f massCenter;
        int firstChild;
        int idxOfFirstPidx, particleCount;
        float appr_threshold; // (size_of_the_box / THETA)^2
    };
    struct BoxPool {
        std::vector<Box> boxes = {{}}; //the root is boxes[0]
        std::atomic<int> boxesUsed{1}; // Thread-safe counter

        BoxPool() {
            boxes.resize(N);
        }

        void reset() { boxesUsed.store(1, std::memory_order_relaxed); }
        int add4Boxes() {
            return boxesUsed.fetch_add(4, std::memory_order_relaxed);
        }
    };

    struct ParticleSystem {
        // Aligned in memory so the CPU can load 8 contiguous X positions at once
        std::vector<float> posX;
        std::vector<float> posY;
        std::vector<float> speedX;
        std::vector<float> speedY;

        ParticleSystem() {
            posX.resize(N);
            posY.resize(N);
            speedX.resize(N, 0.f);
            speedY.resize(N, 0.f);
        }
    };

    BoxPool boxPool;

    sf::RenderWindow window;
    int currentFrame = 0;
    ParticleSystem pS;
    std::vector<int> particle_indices; //permutation of [|0, N-1|]

    sf::Vector2f cameraPos = {-3840.f, -2160.f}; //position of the top left corner of the Camera inside the simulation
    bool dragging = false;
    sf::Vector2i prevMousePos;
    float zoom = 0.25f;
    bool drawingBoxes = false;

    std::vector<unsigned char> density;
    std::vector<unsigned char> pixelData;
    sf::Texture texture;
    sf::Sprite textureSprite;
public:
    Sim() : 
        window(RENDER_ON_WINDOW ? sf::VideoMode({WIDTH, HEIGHT}) : sf::VideoMode({300, 300}), "Gravity simulation", 
        RENDER_ON_WINDOW ? sf::State::Fullscreen : sf::State::Windowed),
        density(WIDTH * HEIGHT),
        pixelData(WIDTH * HEIGHT * 4),
        texture(),
        textureSprite(texture) 
    {
        window.setFramerateLimit(60);

        for (int i = 0; i < N; ++i) {
            float theta = ((float)(rand() % 1000) / 1000.f) * 2.f * 3.1415f;
            float r = (rand() % 2000) + 0.f;
            float speed = sqrtf(G * 0.4f *  r);
            pS.posX[i] = r* cosf(theta);
            pS.posY[i] = r* sinf(theta);
            pS.speedX[i] = speed * -sinf(theta);
            pS.speedY[i] = speed * cos(theta);
            // pS.posX[i] = rand() % 3000 - 1500;
            // pS.posY[i] = rand() % 3000 - 1500;
            // pS.speedX[i] = 0.f;
            // pS.speedY[i] = 0.f; 
            
            particle_indices.push_back(i);
        }

        boxPool.boxes[0].max = {WIDTH, HEIGHT};
        boxPool.boxes[0].min = {0.f, 0.f};

        texture.resize({WIDTH, HEIGHT});          
        textureSprite.setTexture(texture);                    
        textureSprite.setTextureRect(sf::IntRect({0, 0}, sf::Vector2i(WIDTH, HEIGHT)));    
    }

    void drawBoxes(Box &b) {
        std::array<sf::Vertex, 5> lines = {
            sf::Vertex{b.min},
            sf::Vertex{{b.max.x, b.min.y}},
            sf::Vertex{b.max},
            sf::Vertex{{b.min.x, b.max.y}},
            sf::Vertex{b.min}
        };
        for (int i = 0; i < 5; ++i) {
            lines[i].position -= cameraPos;
            lines[i].position *= zoom;
        }
        window.draw(lines.data(), lines.size(), sf::PrimitiveType::LineStrip);
        if (b.firstChild != -1) {
            drawBoxes(boxPool.boxes[b.firstChild]);
            drawBoxes(boxPool.boxes[b.firstChild+1]);
            drawBoxes(boxPool.boxes[b.firstChild+2]);
            drawBoxes(boxPool.boxes[b.firstChild+3]);
        }
    }
    void draw() {
        for (int i = 0; i < WIDTH * HEIGHT; ++i) {
            density[i] = 0;
        }
        for (int i = 0; i < N; ++i) {
            int x = static_cast<int>((pS.posX[i] - cameraPos.x) * zoom);
            int y = static_cast<int>((pS.posY[i] - cameraPos.y) * zoom);

            if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
                if (density[y * WIDTH + x] != 255) {
                    density[y * WIDTH + x]++;
                }                
            }
        }
        
        
        for (int i = 0; i < WIDTH * HEIGHT; ++i) {
            pixelData[4*i + 0] = std::min(255, density[i]*50);
            pixelData[4*i + 1] = std::min(255, density[i]*6);
            pixelData[4*i + 2] = std::min(255, density[i]*3);
            pixelData[4*i + 3] = 255;
        }
        texture.update(pixelData.data());

        if (GENERATE_PNGS) {
            std::ostringstream filename;
            filename << "frames/gravity_" << std::setw(6) << std::setfill('0') << currentFrame++ << ".png";
            sf::Image image = texture.copyToImage();

            if (image.saveToFile(filename.str())) {
                std::cout << "Texture successfully exported!" << std::endl;
            } else {
                std::cerr << "Error: Failed to export texture." << std::endl;
            }
        }
        
        if (RENDER_ON_WINDOW) window.draw(textureSprite);
        

        if (drawingBoxes) drawBoxes(boxPool.boxes[0]);
    }
    void handleUserInput(std::optional<sf::Event> &event) {
        if (const auto* mouseButtonPressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouseButtonPressed->button == sf::Mouse::Button::Left) {
                dragging = true;
                prevMousePos = sf::Mouse::getPosition(window);
            }
        } else if (const auto* mouseButtonPressed = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouseButtonPressed->button == sf::Mouse::Button::Left) {
                dragging = false;
            }
        } else if (dragging) {
            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                cameraPos += sf::Vector2f(prevMousePos - mouseMoved->position) / zoom;
                prevMousePos = mouseMoved->position;
            }
        } else if (const auto* mouseWheelScrolled = event->getIf<sf::Event::MouseWheelScrolled>()) {
            float newZoom = zoom * (1 + mouseWheelScrolled->delta * 0.1f);
            cameraPos += sf::Vector2f(sf::Mouse::getPosition(window)) * (1.f / zoom - 1.f / newZoom);
            zoom = newZoom;
        } else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Space) {
                drawingBoxes = !drawingBoxes;
            }
        }

    }

    void computeAcc(int pIdx, int rootBIdx) {
        // __restrict tells the compiler these arrays won't overlap with anything else
        const float* __restrict posX_ptr = pS.posX.data();
        const float* __restrict posY_ptr = pS.posY.data();
        
        float px = posX_ptr[pIdx];
        float py = posY_ptr[pIdx];
        
        float accX = 0.f;
        float accY = 0.f;

        int stack[64];
        int stackPtr = 0;
        
        stack[stackPtr++] = rootBIdx;

        while (stackPtr > 0) {
            int bIdx = stack[--stackPtr];
            Box &b = boxPool.boxes[bIdx];

            float dx = b.massCenter.x - px;
            float dy = b.massCenter.y - py;
            float dSquared = dx * dx + dy * dy;
            
            if (b.appr_threshold < dSquared) {
                float dist = std::sqrt(dSquared);
                float d_inv = 1.0f / (dist > 10.f ? dist : 10.f);
                float d_cubed_inv = d_inv * d_inv * d_inv;
                
                accX += dx * (G * d_cubed_inv * (float)b.particleCount);
                accY += dy * (G * d_cubed_inv * (float)b.particleCount);
            } else {
                if (b.firstChild == -1) { 
                    
                    int start = b.idxOfFirstPidx;
                    int end = start + b.particleCount;

                    // This tells SIMD exactly how to safely accumulate accX and accY
                    #pragma omp simd reduction(+:accX, accY)
                    for (int j = start; j < end; ++j) {
                        
                        // Use the raw pointers 
                        float pdx = posX_ptr[j] - px;
                        float pdy = posY_ptr[j] - py;
                        float pDistSq = pdx * pdx + pdy * pdy;
                        
                        float pDist = std::sqrt(pDistSq);                        
                        float pd_inv = 1.0f / (pDist > 10.f ? pDist : 10.f);                           
                        float pd_cubed_inv = pd_inv * pd_inv * pd_inv;
                        
                        accX += pdx * (G * pd_cubed_inv);
                        accY += pdy * (G * pd_cubed_inv);
                    }
                } else {
                    stack[stackPtr++] = b.firstChild;
                    stack[stackPtr++] = b.firstChild + 1;
                    stack[stackPtr++] = b.firstChild + 2;
                    stack[stackPtr++] = b.firstChild + 3;
                }
            }
        }

        pS.speedX[pIdx] += accX;
        pS.speedY[pIdx] += accY;
    }
    void applyPhysic() {
        #pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < N; ++i) {
            computeAcc(i, 0);
        }

        #pragma omp parallel for
        for (int i = 0; i < N; ++i) {
            pS.posX[i] += pS.speedX[i];
            if (pS.posX[i] > MAX_BOUNDARIES.x || pS.posX[i] < MIN_BOUNDARIES.x) {
                pS.posX[i] -= pS.speedX[i];
                pS.speedX[i] *= -1;
            }
            pS.posY[i] += pS.speedY[i];
            if (pS.posY[i] > MAX_BOUNDARIES.y || pS.posY[i] < MIN_BOUNDARIES.y) {
                pS.posY[i] -= pS.speedY[i];
                pS.speedY[i] *= -1;
            }
        }
    }
    
    
    void buildTree() {
        Box &root = boxPool.boxes[0];
        root.idxOfFirstPidx = 0;
        root.particleCount = N;

        root.min = {pS.posX[0], pS.posY[0]};
        root.max = {pS.posX[0], pS.posY[0]};

        for (int i = 1; i < N; ++i) {
            root.min.x = (root.min.x < pS.posX[i]) ? root.min.x : pS.posX[i];
            root.min.y = (root.min.y < pS.posY[i]) ? root.min.y : pS.posY[i];
            
            root.max.x = (root.max.x > pS.posX[i]) ? root.max.x : pS.posX[i];
            root.max.y = (root.max.y > pS.posY[i]) ? root.max.y : pS.posY[i];
        }

        boxPool.reset();
        
        #pragma omp parallel
        {
            #pragma omp single
            {
                splitBox(0);
            }
        }
        //std::cout << "max: " << boxPool.boxes[0].max.x << ", " << boxPool.boxes[0].max.y << "|min: " << boxPool.boxes[0].min.x << ", " << boxPool.boxes[0].min.y << std::endl;
        ParticleSystem newPS;

        #pragma omp parallel for
        for (int i = 0; i < N; ++i) {
            newPS.posX[i] = pS.posX[particle_indices[i]];
            newPS.posY[i] = pS.posY[particle_indices[i]];
            newPS.speedX[i] = pS.speedX[particle_indices[i]];
            newPS.speedY[i] = pS.speedY[particle_indices[i]];
        }

        pS = std::move(newPS);

        #pragma omp parallel for
        for (int i = 0; i < N; ++i) {
            particle_indices[i] = i;
        }
    }
    void computeLeafProperties(int bIdx) {
        Box &b = boxPool.boxes[bIdx];
        b.appr_threshold = ( (b.max.x - b.min.x > b.max.y - b.min.y) ? 
                            (b.max.x - b.min.x)*(b.max.x - b.min.x) : 
                            (b.max.y - b.min.y)*(b.max.y - b.min.y) ) / THETA_SQUARED;
        
        b.massCenter = {0.f, 0.f};
        b.firstChild = -1;
        for (int j = b.idxOfFirstPidx; j < b.idxOfFirstPidx + b.particleCount; ++j) {
            b.massCenter += {pS.posX[particle_indices[j]], pS.posY[particle_indices[j]]};
        }
        if (b.particleCount > 0) {
            b.massCenter /= (float)b.particleCount;
        }
    }

    template <typename Predicate>
    int partition(int first, int afterLast, Predicate predicate) {
        int i = first;
        int j = afterLast - 1;

        while (i <= j) {
            if (predicate(particle_indices[i])) {
                std::swap(particle_indices[i], particle_indices[j]);
                --j;
            } else {
                ++i;
            }
        }

        return i;
    }
    void splitBox(int bIdx) {
        sf::Vector2f mid = (boxPool.boxes[bIdx].max + boxPool.boxes[bIdx].min) / 2.f;
        if (mid.y - boxPool.boxes[bIdx].min.y < MIN_LEAF_WIDTH || mid.x - boxPool.boxes[bIdx].min.x < MIN_LEAF_WIDTH) {
            computeLeafProperties(bIdx);
            return;
        }

        int b1Idx = boxPool.add4Boxes();
        int b2Idx = b1Idx + 1;
        int b3Idx = b1Idx + 2;
        int b4Idx = b1Idx + 3;

        boxPool.boxes[bIdx].firstChild = b1Idx;

        boxPool.boxes[b1Idx].max = mid;
        boxPool.boxes[b1Idx].min = boxPool.boxes[bIdx].min;

        boxPool.boxes[b2Idx].max = {boxPool.boxes[bIdx].max.x, mid.y};
        boxPool.boxes[b2Idx].min = {mid.x, boxPool.boxes[bIdx].min.y};

        boxPool.boxes[b3Idx].max = {mid.x, boxPool.boxes[bIdx].max.y};
        boxPool.boxes[b3Idx].min = {boxPool.boxes[bIdx].min.x, mid.y};

        boxPool.boxes[b4Idx].max = boxPool.boxes[bIdx].max;
        boxPool.boxes[b4Idx].min = mid;

        
        int first = boxPool.boxes[bIdx].idxOfFirstPidx;
        int afterLast = first + boxPool.boxes[bIdx].particleCount;

        int xSplit = partition(first, afterLast, [&](int i) {return pS.posX[i] > mid.x; });
        int ySplit_1_3 = partition(first, xSplit, [&](int i) {return pS.posY[i] > mid.y; });
        int ySplit_2_4 = partition(xSplit, afterLast, [&](int i) {return pS.posY[i] > mid.y; });

        boxPool.boxes[b1Idx].idxOfFirstPidx = first;
        boxPool.boxes[b1Idx].particleCount = ySplit_1_3 - first;

        boxPool.boxes[b2Idx].idxOfFirstPidx = xSplit;
        boxPool.boxes[b2Idx].particleCount = ySplit_2_4 - xSplit;

        boxPool.boxes[b3Idx].idxOfFirstPidx = ySplit_1_3;
        boxPool.boxes[b3Idx].particleCount = xSplit - ySplit_1_3;

        boxPool.boxes[b4Idx].idxOfFirstPidx = ySplit_2_4;
        boxPool.boxes[b4Idx].particleCount = afterLast - ySplit_2_4;


        bool spawnTasks = boxPool.boxes[bIdx].particleCount > 1000;

        int children[4] = {b1Idx, b2Idx, b3Idx, b4Idx};

        for (int childIdx : children) {
            if (boxPool.boxes[childIdx].particleCount > MAX_PARTICLE_IN_LEAF) {
                if (spawnTasks) {
                    // Spawn a parallel task for each branch
                    #pragma omp task firstprivate(childIdx)
                    splitBox(childIdx);
                } else {
                    // Execute sequentially when below threshold
                    splitBox(childIdx);
                }
            } else {
                computeLeafProperties(childIdx);
            }
        }

        // Wait for all child tasks of this node to finish before returning
        if (spawnTasks) {
            #pragma omp taskwait
        }

        Box &b = boxPool.boxes[bIdx];
       
        // Compute internal node properties since all children are completely done
        b.appr_threshold = ( (b.max.x - b.min.x > b.max.y - b.min.y) ? 
                            (b.max.x - b.min.x)*(b.max.x - b.min.x) : 
                            (b.max.y - b.min.y)*(b.max.y - b.min.y) ) / THETA_SQUARED;

        b.massCenter = {0.f, 0.f};
        for (int i = 0; i < 4; ++i) {
            Box &child = boxPool.boxes[b.firstChild + i];
            if (child.particleCount > 0) {
                b.massCenter += (float)child.particleCount * child.massCenter;
            }
        }
        if (b.particleCount > 0) {
            b.massCenter /= (float)b.particleCount;
        }
    }

    void main_loop() {
        while (window.isOpen()) {
            while (std::optional<sf::Event> event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>())
                    window.close();

                handleUserInput(event);
            }
            auto begin = std::chrono::steady_clock::now();
            buildTree();
            auto end = std::chrono::steady_clock::now();
            std::cout << "buildTree " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << " us";
            std::cout << "  boxesUsed: " << boxPool.boxesUsed;

            
            boxesVisited = 0, approximations = 0, directInteractions = 0;
            begin = std::chrono::steady_clock::now();
            applyPhysic();
            end = std::chrono::steady_clock::now();
            std::cout << " | applyPhysic " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << " us";
            std::cout << " bVisited: " << boxesVisited << " approx: " << approximations << " directI: " << directInteractions;


            window.clear();

            begin = std::chrono::steady_clock::now();
            draw();
            end = std::chrono::steady_clock::now();
            std::cout << " | draw " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << " us" << std::endl;
            
            window.display();
        }
    }
};

int main() {
    Sim s;
    s.main_loop();
    return 0;
}