/* 
   An IoT temperature logger in the internet domain using TCP and SQLite3
   Stephen R. Williams, 8th Dec 2018
   Open source GPLv3 as specified at repository
*/

var myChart;
const url1 = '/ttdata';
const url2 = '/data.json';
const url3 = '/range.json';



////////// jQuery date picker interface ////////////////
setFromMin = null;
setFromMax = null;
setToMin = null;
setToMax = null;

$( function() {
    //var dateFormat = "dd/mm/yy",
    from = $("#from").datepicker({
        dateFormat: "dd/mm/yy",
        //defaultDate: "+1w",
        changeMonth: true,
        //numberOfMonths: 3
    })
    .on( "change", function(){
        to.datepicker("option", "minDate", getDate(this));
    }),
    to = $("#to").datepicker({
        dateFormat: "dd/mm/yy",
        //defaultDate: "+1w",
        changeMonth: true,
        //numberOfMonths: 3
    })
    .on( "change", function(){
        from.datepicker("option", "maxDate", getDate(this));
    });
    
    function getDate(element) { // for internal use
        var date;
        try {
            date = $.datepicker.parseDate( "dd/mm/yy", element.value );
        } catch( error ) {
            date = null;
        }
        return date;
    }

    function set_fromMax(dd){
        var date = new Date(dd);
        from.datepicker("option", "maxDate", date);
    }
    setFromMax = set_fromMax;
    function set_fromMin(dd){
        var date = new Date(dd);
        from.datepicker("option", "minDate", date);
    }
    setFromMin = set_fromMin;
    function set_toMax(dd){
        var date = new Date(dd);
        to.datepicker("option", "maxDate", date);
    }
    setToMax = set_toMax;
    function set_toMin(dd){
        var date = new Date(dd);
        to.datepicker("option", "minDate", date);
    }
    setToMin = set_toMin;
} );

function formatDate(tag){
    var date;
    try {
        date = $(tag).datepicker("getDate");
        date = $.datepicker.formatDate("yy-mm-dd", date);
    } catch( error ) {
        date = null;
    }
    return date;
}
/// End jQuery interface ///////////////////////////////


// Object containing data and graph //////////////////////
var gdata = {
    date_smallest: "",
    date_largest: "",
    
    // function to renew the graph data
    updateData(){
        var x = "select * from tbl1 where time between '";
        x += this.date_smallest + "T00:00' and '" + this.date_largest +"T24:00'";
        //Get the json file with fetch
        fetch(url2, {  // send date data after header
            method:'post',
            headers: {
                "Content-type": "application/dates; charset=UTF-8"
            },
            body: x
        })
        .then((response) => {
            return response.json();
        }).then((myJson) => {
            myChart.data.datasets[0].data = myJson;       
            myChart.update();
        }).catch((error) => {
            console.error(error);    
        });
    },
    
    drawChart(jsonData) {
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
    },

    // function to get current time and temperature and write it to "mydata"
    displayResult(){
        fetch(url1).then((response) => {
            if(response.ok){
                response.text().then((myText) => {
                    //str = myText;
                    document.getElementById("mydata").innerHTML = myText;
                });
            }else{
                console.log('Network request failed with response ' + response.status + ': ' + response.statusText);
            }
        });     
    },

    // function called to set chosen date limits
    dateFunc(){
        var x = formatDate("#from");
        var y = formatDate("#to");
        var d1 = new Date(x);
        var d2 = new Date(y);
        // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleDateString
        // location codes: https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry
        var ds1 = d1.toLocaleDateString('en-AU');
        var ds2 = d2.toLocaleDateString('en-AU');
        document.getElementById("date_range").innerHTML = "Date range setting: " + ds1 + "  -  " + ds2;
        document.getElementById("down_load").href = "graph.csv?" + x + "T00:00_" + y + "T24:00";
        this.date_smallest = x;
        this.date_largest = y;
    },
    
    initDateRange(){
        fetch(url3)
        .then((response) => {
            return response.json();
        }).then((myJson) => {
            this.date_smallest = myJson.min;
            this.date_largest = myJson.max;
            $("#to").datepicker("setDate", new Date(this.date_largest));
            $("#from").datepicker("setDate", new Date(this.date_smallest));
            setToMin(this.date_smallest);
            setToMax(this.date_largest);
            setFromMin(this.date_smallest);
            setFromMax(this.date_largest);
            this.dateFunc();
        }).catch((error) => {
            console.error(error);    
        });    
    },

    ResetDateMax(){
        fetch(url3)
        .then((response) => {
            return response.json();
        }).then((myJson) => {
            this.date_largest = myJson.max;
            setToMax(this.date_largest);
            //this.dateFunc();
        }).catch((error) => {
            console.error(error);    
        });    
    },

    // get data, draw graph, update time
    draw(){
        // display current time and temperature
        gdata.displayResult();
        // set initial date range
        gdata.initDateRange();
        // Get the json file with fetch, then call drawChart()
        fetch(url2)
        .then((response) => {
            return response.json();
        }).then((myJson) => {
            gdata.drawChart(myJson);  // Pass the json data to drawChart
        }).catch((error) => {
            console.error(error);    
        });
        // update time and temperature display every 5 mins
        setInterval(gdata.displayResult, 300000);
        // unpate maximum datepicker setting every 10 mins & 127 secs, don't ask both at once all the time
        setInterval(gdata.ResetDateMax, 999983);
    }
};

// wait for HTML to load before loading graph, calls draw()
window.addEventListener("DOMContentLoaded", gdata.draw); 
//displayResult(); // display result on initial load




