function requestGraph() {
    $.ajax({
        url: '/graph',
        success: function(response) {
            console.log(response)
            $("#graph-container").html(response);

            // This is the refresh
            setTimeout(requestGraph,5000);
        },
        cache: false
    })
}

function requestAlerts() {
    $.ajax({
        url: '/alerts',
        success: function(response) {
            var html = "<tr>"
            //debugger
            for(var i in response) {
                alert = response[i]
                // Fix how the html gets generated once you know the format of the alerts
                html = "<tr><td>" + alert["id"] + "</td><td>" + alert["one"] + "</td><td>" + alert["two"] + "</td><td>" + alert["three"] + "</td><td>" + alert["four"] + "</td></tr>";
                $("#alert_body").append(html)
            }

            // This is the refresh
            setTimeout(requestAlerts,5000);
        },
        cache: false
    })

}


$(document).ready(function() {
    console.log("Document ready");
    requestGraph();
    requestAlerts();
})