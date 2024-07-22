document.addEventListener('DOMContentLoaded', (event) => {
    var bpmCtx = document.getElementById('bpmChart').getContext('2d');
    var tempCtx = document.getElementById('tempChart').getContext('2d');

    var bpmChart = new Chart(bpmCtx, {
        type: 'line',
        data: {
            labels: Array.from({ length: 60 }, (_, i) => i),
            datasets: [{
                label: 'BPM',
                data: [],
                borderColor: 'rgb(231, 76, 60)',
                tension: 0.1,
                borderWidth: 2,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: false,
                    min: 60,
                    max: 110,
                    ticks: {
                        stepSize: 10
                    }
                },
                x: {
                    display: false
                }
            },
            plugins: {
                legend: {
                    display: false
                }
            },
            animation: {
                duration: 0
            }
        }
    });

    var tempChart = new Chart(tempCtx, {
        type: 'line',
        data: {
            labels: Array.from({ length: 60 }, (_, i) => i),
            datasets: [{
                label: 'Temperatura',
                data: [],
                borderColor: 'rgb(52, 152, 219)',
                tension: 0.1,
                borderWidth: 2,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: false,
                    min: 23, // Ajusta estos valores según tu rango esperado
                    max: 40, // Ajusta estos valores según tu rango esperado
                    ticks: {
                        stepSize: 1
                    }
                },
                x: {
                    display: false
                }
            },
            plugins: {
                legend: {
                    display: false
                }
            },
            animation: {
                duration: 0
            }
        }
    });

    var socket = new WebSocket('ws://192.168.11.151:8080');

    socket.onmessage = function (event) {
        var data = JSON.parse(event.data);

        if (!isNaN(data.bpm) && data.bpm >= bpmRange.min && data.bpm <= bpmRange.max) {
            bpmChart.data.datasets[0].data.push(data.bpm);
            if (bpmChart.data.datasets[0].data.length > 60) {
                bpmChart.data.datasets[0].data.shift();
            }
            bpmChart.update();

            document.getElementById('bpm').textContent = data.bpm.toFixed(2);
        }

        if (!isNaN(data.temp)) {
            tempChart.data.datasets[0].data.push(data.temp);
            if (tempChart.data.datasets[0].data.length > 60) {
                tempChart.data.datasets[0].data.shift();
            }
            tempChart.update();

            document.getElementById('temp').textContent = data.temp.toFixed(2);
        }

        // Actualizar otros valores sin filtrar
        ['p', 'qrs', 't', 'pr', 'qt', 'qtc'].forEach(key => {
            if (!isNaN(data[key])) {
                document.getElementById(key).textContent = data[key].toFixed(2);
            }
        });
    };

    document.getElementById('historyButton').addEventListener('click', function () {
        fetch('/history')
            .then(response => response.json())
            .then(data => {
                var tbody = document.getElementById('historyTable').querySelector('tbody');
                tbody.innerHTML = ''; // Limpiar la tabla antes de llenarla de nuevo
    
                // Rellenar la tabla con los últimos 10 registros
                for (var i = 0; i < data.timestamps.length; i++) {
                    var row = document.createElement('tr');
    
                    var timestampCell = document.createElement('td');
                    var date = new Date(data.timestamps[i] * 1000);
                    timestampCell.textContent = date.toLocaleTimeString();
                    row.appendChild(timestampCell);
    
                    ['bpm', 'temp', 'p', 'qrs', 't', 'pr', 'qt', 'qtc'].forEach(key => {
                        var cell = document.createElement('td');
                        cell.textContent = !isNaN(data[key][i]) ? data[key][i].toFixed(2) : '--';
                        row.appendChild(cell);
                    });
    
                    tbody.appendChild(row);
                }
    
                document.getElementById('historyTable').style.display = 'table';
            })
            .catch(error => console.error('Error fetching history data:', error));
    });
});