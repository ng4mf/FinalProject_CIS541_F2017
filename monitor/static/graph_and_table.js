function requestGraph() {
    $.ajax({
        url: '/graph',
        success: function(response) {
            console.log(response)
            $("#graph-container").html(response);

            // This is the refresh
            setTimeout(requestGraph,1000);
        },
        cache: false
    })
}

$(document).ready(function() {
    console.log("Document ready");
    requestGraph();
})