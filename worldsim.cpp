#include <mpi.h>
#include <iostream>
#include <vector>
#include <random>

// Estructura del Jugador
struct EstadoJugador {
    int id;
    float x;
    float y;
    int vida;
};

// Funcion que dividira el mapa total segun cantidad de nodos
int obtener_rango_por_zona(float x, float y, int total_nodos) {
    // Si hay 1 solo nodo, maneja todo el mapa
    if (total_nodos == 1) {
        return 0; 
    }
    // Si hay 2 nodos, dividimos el mapa a la mitad verticalmente (X=50)
    else if (total_nodos == 2) {
        if (x < 50.0f) return 0; // Mitad Izquierda
        else return 1;           // Mitad Derecha
    }
    // Si hay 4, cuadrícula 2x2 
    else if (total_nodos == 4) {
        if (x < 50.0f && y < 50.0f) return 0; // Noroeste
        if (x >= 50.0f && y < 50.0f) return 1; // Noreste
        if (x < 50.0f && y >= 50.0f) return 2; // Suroeste
        if (x >= 50.0f && y >= 50.0f) return 3; // Sureste
    }
    return -1; //Out of bounds
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank_id, total_nodos;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_id);
    MPI_Comm_size(MPI_COMM_WORLD, &total_nodos);

    // Validamos que se ejecute con 1, 2 o 4 nodos para que las priebas sean exactas
    if (total_nodos != 1 && total_nodos != 2 && total_nodos != 4) {
        if (rank_id == 0) {
            std::cerr << "Error: Por favor ejecuta con 1, 2 o 4 nodos para la prueba.\n";
        }
        MPI_Finalize();
        return 1;
    }

    std::vector<EstadoJugador> jugadores_locales;
    int total_jugadores = 10000; // Carga masiva para el procesador
    int ticks_simulacion = 500;  // Iteraciones del Game Loop

    // El Rank 0 genera todos los jugadores inicialmente en su zona (X de 0 a 49, Y de 0 a 49)
    if (rank_id == 0) {
        std::mt19937 gen(42); // Fixed seed para que la prueba sea idéntica cada vez
        std::uniform_real_distribution<float> dis_pos(0.0f, 40.0f);
        
        for (int i = 0; i < total_jugadores; ++i) {
            jugadores_locales.push_back({i, dis_pos(gen), dis_pos(gen), 100});
        }
        std::cout << "--- BENCHMARK: MUNDO CONTINUO (MEMORIA DISTRIBUIDA) ---\n";
        std::cout << "Nodos en el clúster: " << total_nodos << "\n";
        std::cout << "Entidades totales: " << total_jugadores << "\n";
        std::cout << "Simulando físicas y traspasos por " << ticks_simulacion << " ticks...\n";
    }

    // Sincronizamos todos los nodos
    MPI_Barrier(MPI_COMM_WORLD);
    double tiempo_inicio = MPI_Wtime();

    // GAME LOOP
    for (int tick = 1; tick <= ticks_simulacion; ++tick) {
        
        // 1 -. Recibir handoffs que esten en cola
        int hay_mensaje = 0;
        MPI_Status status;
        do {
            MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &hay_mensaje, &status);
            if (hay_mensaje) {
                EstadoJugador jugador_entrante;
                MPI_Recv(&jugador_entrante, sizeof(EstadoJugador), MPI_BYTE, 
                         status.MPI_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                jugadores_locales.push_back(jugador_entrante);
            }
        } while (hay_mensaje); // vaciar el buffer de red

        // 2 y 3: Calcular Físicas y Detectar fronteras
        for (auto it = jugadores_locales.begin(); it != jugadores_locales.end(); ) {
            // Carga de procesador: Movimiento diagonal para forzarlos a cruzar fronteras
            it->x += 0.2f;
            it->y += 0.2f; 
            
            // Simular cálculos pesados de IA (para estresar la CPU y ver la mejora del clúster)
            float calculo_dummy = (it->x * it->y) / (it->vida + 1.0f);
            it->vida = 100 - (int)(calculo_dummy * 0.0001f);

            // Detección de límites
            int zona_correcta = obtener_rango_por_zona(it->x, it->y, total_nodos);
            
            // Si el jugador ya no pertenece a este nodo
            if (zona_correcta != rank_id && zona_correcta != -1) {
                // Traspaso: Enviar estado por red
                MPI_Send(&(*it), sizeof(EstadoJugador), MPI_BYTE, zona_correcta, 0, MPI_COMM_WORLD);
                // Eliminar localmente
                it = jugadores_locales.erase(it);
            } else {
                ++it;
            }
        }
        
        // Sincronización para mantener los ticks parejos
        MPI_Barrier(MPI_COMM_WORLD); 
    }

    // --- FIN DEL BENCHMARK ---
    MPI_Barrier(MPI_COMM_WORLD);
    double tiempo_fin = MPI_Wtime();

    // Imprimir resultados
    if (rank_id == 0) {
        std::cout << "\n--- Resultados Finales ---\n";
        std::cout << "TIEMPO TOTAL DE EJECUCIÓN: " << (tiempo_fin - tiempo_inicio) << " segundos.\n";
        std::cout << "--------------------------\n\n";
    }

    MPI_Finalize();
    return 0;
}
