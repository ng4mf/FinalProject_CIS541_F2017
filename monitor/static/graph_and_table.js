function requestGraph() {
    $.ajax({
        url: '/graph',
        success: function(response) {
            console.log(response)
            $("#graph-container").html(response);

            // This is the refresh
            setTimeout(requestGraph,3000);
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
            console.log(response)
            $("#alert_body").empty();
            for(var i in response) {
                alert = response[i]
                // Fix how the html gets generated once you know the format of the alerts
                html = "<tr><td>" + alert["timestamp"] + "</td><td>" + alert["type"] + "</td></tr>";
                $("#alert_body").append(html)
            }

            // This is the refresh
            setTimeout(requestAlerts,3000);
        },
        cache: false
    })

}


$(document).ready(function() {
    console.log("Document ready");
    requestGraph();
    requestAlerts();
})
