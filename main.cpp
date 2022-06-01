#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <sstream>
#include <chrono>
#include <numeric>
#include <thread>
#include <cmath>
#include <future>
using namespace std;
using namespace std::chrono;

const int expected_range = 25;

void generar_archivo(int n, const string& file_name, int max_value) {
    ofstream f_out(file_name);
    random_device rd;
    uniform_int_distribution<int> dis(-max_value, max_value);
    for (int i = 0; i < n; ++i)
        f_out << dis(rd) << " ";
}

vector<int> generar_vector(const string& file_name) {
    vector<int> result;
    ifstream fin(file_name);
    if (!fin.is_open())
        return vector<int>{};
    stringstream ss_fin;
    ss_fin << fin.rdbuf();
    int value;
    while (ss_fin >> value)
        result.push_back(value);
    return result;
}

using time_point_t = time_point<high_resolution_clock>;
class timer_t {
    time_point_t start;
    time_point_t stop;
public:
    timer_t() { start = high_resolution_clock::now(); }
    ~timer_t() {
        stop = high_resolution_clock::now();
        cout    << "tiempo: "
                << duration_cast<microseconds>(stop - start).count()
                << " microsegundos\n";
    }
};

template <typename Iterator>
void acumular(Iterator start, Iterator stop, Iterator result) {
    *result = accumulate(start, stop, 0);
}

int get_number_of_threads(int sz, int rng) {
    int max_threads = (sz + rng - 1) / rng;
    int k_threads = static_cast<int>(thread::hardware_concurrency());
    return min(k_threads != 0? k_threads: 2, max_threads);
}

template <typename T, template<typename...> typename CType>
T acumulador_paralelo(CType<T>& data) {
    // Calculando el número de hilos
    int n_hilos = get_number_of_threads(size(data), expected_range);
    auto rango = size(data) / n_hilos;
    // Vector de hilos
    vector<thread> vec_hilos (n_hilos);
    vector<T> res_hilos (n_hilos);
    // Iteradores
    auto it_curr_data = begin(data);
    auto it_res_hilos = begin(res_hilos);
    // Recorrer los hilos
    // Map
    for (auto& item: vec_hilos) {
        if (distance(it_curr_data, end(data)) < rango)
            rango = distance(it_curr_data, end(data));
        item = thread(acumular<decltype(it_curr_data)>,
                      it_curr_data,
                      next(it_curr_data, rango),
                      it_res_hilos);
        it_curr_data = next(it_curr_data, rango);
        ++it_res_hilos;
    }
    // Union de los hilos
    for (auto& item: vec_hilos)
        item.join();

    // Reduction
    return accumulate(begin(res_hilos), end(res_hilos), 0);
}

template <typename T, template<typename...> typename CType>
T acumulador_paralelo_async(CType<T>& data) {
    // Calculando el número de hilos
    int n_hilos = get_number_of_threads(size(data), expected_range);
    auto rango = size(data) / n_hilos;
    // Vector de hilos
    vector<future<T>> vec_futures (n_hilos);
    // Iteradores
    auto it_curr_data = begin(data);
    // Recorrer los hilos
    // Map
    for (auto& item: vec_futures) {
        if (distance(it_curr_data, end(data)) < rango)
            rango = distance(it_curr_data, end(data));
        item = async(accumulate<decltype(it_curr_data), T>,
                      it_curr_data,
                      next(it_curr_data, rango),
                      0);
        it_curr_data = next(it_curr_data, rango);
    }
    // Reduction
    T total = 0;
    for (auto& ftr: vec_futures) total += ftr.get();
    return total;
}

template <typename Iterator, typename T = typename Iterator::value_type>
void acumulador_rec_(Iterator start, size_t size, int range, T& total) {
    if (size < range) {
        total = accumulate(start, next(start, size), 0);
        return;
    }
    auto a = T{};
    thread t1(acumulador_rec_<Iterator>, start, size / 2, range, ref(a));
    auto b = T{};
    thread t2(acumulador_rec_<Iterator>, next(start, size / 2), size - size / 2, range, ref(b));
    t1.join();
    t2.join();
    total = a + b;
}

template <typename T, template<typename...> typename CType>
T acumulador_recursivo(CType<T> data) {
    int n_hilos = get_number_of_threads(size(data), expected_range);
    auto rango = size(data) / n_hilos;
    auto total = T{};
    acumulador_rec_(begin(data), size(data), rango, total);
    return total;
}

template <typename Iterator>
auto acumulador_rec_sync_(Iterator start, size_t size, int range) {
    if (size < range) {
        return accumulate(start, next(start, size), 0);
    }
    auto a = async(acumulador_rec_sync_<Iterator>, start, size / 2, range);
    auto b = async(acumulador_rec_sync_<Iterator>, next(start, size / 2), size - size / 2, range);
    return a.get() + b.get();
}

template <typename T, template<typename...> typename CType>
T acumulador_recursivo_async(CType<T> data) {
    int n_hilos = get_number_of_threads(size(data), expected_range);
    auto rango = size(data) / n_hilos;
    return acumulador_rec_sync_(begin(data), size(data), rango);
}

int main() {
//    generar_archivo(1000, "../datos_1000.txt");
//    generar_archivo(100000, "../datos_100000.txt");
//    generar_archivo(1000000, "../datos_1000000.txt");
//    generar_archivo(10000000, "../datos_10000000.txt");
//    generar_archivo(90000, "../datos_90000.txt", 25000);
    vector<int> datos;
    {
        cout << "Generando vector...\n";
        timer_t t;
        datos = generar_vector("../datos_10000000.txt");
        cout << "Size de vector: " << size(datos) << endl;
    }

    {
        cout << "Calcular suma en forma secuencial o serial...\n";
        timer_t t;
        cout << "Total: " << accumulate(begin(datos), end(datos), 0) << endl;
    }

    {
        cout << "Calcular suma en forma paralela...\n";
        timer_t t;
        cout << "Total: " << acumulador_paralelo(datos) << endl;
    }
    {
        cout << "Calcular suma en forma paralela usando async...\n";
        timer_t t;
        cout << "Total: " << acumulador_paralelo_async(datos) << endl;
    }

    {
        cout << "Calcular suma en forma paralela recursiva async...\n";
        timer_t t;
        cout << "Total: " << acumulador_recursivo(datos) << endl;
    }
    {
        cout << "Calcular suma en forma paralela recursiva async...\n";
        timer_t t;
        cout << "Total: " << acumulador_recursivo_async(datos) << endl;
    }
    return 0;
}
