#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <queue> // Para la cola de tareas

// --- Memoria Compartida ---
int sesiones_activas = 0;
std::vector<int> registro_usuarios;
std::mutex mtx; // proteger la memoria (sesiones y registro)

std::queue<int> cola_usuarios; // Cola de usuarios esperando iniciar sesión
std::mutex mtx_cola;           // proteger la extracción de la cola

// Simula la validación en base de datos y el inicio de sesión
void procesar_login(int user_id) {
    // Simulamos un tiempo de procesamiento aleatorio (latencia)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10, 50); // Simula entre 10ms y 50ms de latencia
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

    // Sección Crítica
    {
        std::lock_guard<std::mutex> lock(mtx); // Bloquea el mutex al entrar, lo libera al salir
        sesiones_activas++;
        registro_usuarios.push_back(user_id);
    } // se libera el lock_guard
}

// ---(Worker)---
void hilo_trabajador(int id_nucleo) {
    while (true) {
        int user_id = -1;
        
        // Sección crítica de la cola: Sacar un usuario de forma segura
        {
            std::lock_guard<std::mutex> lock(mtx_cola);
            if (cola_usuarios.empty()) {
                break; // Si la cola está vacía, el núcleo terminó su trabajo
            }
            user_id = cola_usuarios.front();
            cola_usuarios.pop();
        }

        // Procesar el login (Fuera del candado de la cola para permitir paralelismo)
        if (user_id != -1) {
            procesar_login(user_id);
        }
    }
}

int main(int argc, char* argv[]) {

    int cantidad_nucleos = 1; 
    int total_usuarios = 500; // Subimos a 500 para notar bien la diferencia de tiempo
    
    if (argc > 1) {
        cantidad_nucleos = std::stoi(argv[1]);
    }

    std::cout << "--- BENCHMARK DE LOGIN ---\n";
    std::cout << "Usuarios totales a loguear: " << total_usuarios << "\n";
    std::cout << "Hilos (Threads) utilizados: " << cantidad_nucleos << "\n";
    std::cout << "Procesando... por favor espere.\n";
    
    // Llenamos la cola de peticiones con todos los usuarios
    for (int i = 1; i <= total_usuarios; ++i) {
        cola_usuarios.push(i);
    }

    std::vector<std::thread> hilos;

    // Lanzamos los hilos
    for (int i = 1; i <= cantidad_nucleos; ++i) {
        hilos.push_back(std::thread(hilo_trabajador, i));
    }

    // Esperamos a que todos los núcleos terminen
    for (auto& hilo : hilos) {
        if (hilo.joinable()) {
            hilo.join();
        }
    }
    
    return 0;
}
