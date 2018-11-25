var myChart;
const url1 = '?data';
const url2 = '/data.json';

// wait for basic page to load before loading graph, calls draw()
window.addEventListener("DOMContentLoaded", draw); 

displayResult(); // display result on initial load

function displayResult(){
    fetch(url1).then((response) => {
        if(response.ok){
            response.text().then((myText) => {
                str = myText;
                document.getElementById("mydata").innerHTML = myText;
            });
        }else{
            console.log('Network request failed with response ' + response.status + ': ' + response.statusText);
        }
    });     
}

// function to renew the graph data
function updateData(){
    //Get the json file with fetch
    fetch(url2)
    .then((response) => {
        return response.json();
    }).then((myJson) => {
        myChart.data.datasets[0].data = myJson;       
        myChart.update();
    }).catch((error) => {
        console.error(error);    
    });
}

function draw(){
    // Get the json file with fetch
    fetch(url2)
    .then((response) => {
        return response.json();
    }).then((myJson) => {
        drawChart(myJson);  // Pass the json data to drawChart
    }).catch((error) => {
        console.error(error);    
    });
}

function drawChart(jsonData) {
    // making a graph with chart.js
    moment().utcOffset(0); // set the timezone offset to zero
    var ctx = document.getElementById("myChart").getContext('2d');
    myChart = new Chart(ctx, {
        type: 'line',
        data: {
            datasets: [{
                //label: 'Scatter Dataset',
                data: jsonData,
                borderColor: 'Blue',
                fill: false,
                pointRadius: 0,
                lineTension: 0.1,
                borderWidth: 4,
            }]
        },
        options: {
            scales: {
                xAxes: [{
                    type: 'time',
                    time: { unit: 'day' },
                    distribution: 'linear',
                    position: 'bottom',
                    scaleLabel: {
                        display: true,
                        fontSize: 20,
                        labelString: 'time'
                    }
                }],
                yAxes: [{
                    scaleLabel: {
                        display: true,
                        fontSize: 20,
                        labelString: 'temperature (degrees Celsius)'
                    }
                }]
            },
            legend: { display: false }
        }
    });
}

