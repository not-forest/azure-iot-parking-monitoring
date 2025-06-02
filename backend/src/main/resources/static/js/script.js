const socket = new SockJS('/ws');
const stompClient = Stomp.over(socket);

document.addEventListener("DOMContentLoaded", function () {
  const date = new Date();
  const fullDate = `Dzisiaj: ${date.getDate()}/${date.getMonth() + 1}/${date.getFullYear()}`;

  document.querySelectorAll('[data-bs-toggle="popover"]').forEach(label => {
    label.setAttribute("data-bs-content", fullDate);
    new bootstrap.Popover(label);
  });
});
stompClient.connect({}, function(frame) {
    console.log('Connected: ' + frame);

    stompClient.subscribe('/topic/parking', function(message) {
        const data = JSON.parse(message.body);
        const label = document.querySelector('label[for="1A"]');
        if (label) {
            if (data.is_free === false) {
                label.classList.add("occupied");
                label.classList.remove("free");
            } else {
                label.classList.add("free");
                label.classList.remove("occupied");
            }
        }
    });
});
const controllerData = {

  "1B": false,
  "1C": false,
  "2A": false,
  "2B": false,
  "2C": false,
  "2D": false,
  "3A": false,
  "3B": false,
  "3C": false,
  "3D": false,
  "4A": false,
  "4B": false,
  "4C": false,
  "4D": false,
  "5A": false,
  "5B": false,
  "5C": false,
  "5D": false,
  "6A": false,
  "6B": false,
  "6C": false,
  "6D": false,
};

function updateAllSpotColors() {
  for (const spotId in controllerData) {
    const label = document.querySelector(`label[for="${spotId}"]`);
    if (!label) continue;

    const status = controllerData[spotId];
    if (status === false) {
      label.classList.add("free");
      label.classList.remove("occupied");
    } else {
      label.classList.add("occupied");
      label.classList.remove("free");
    }
  }
}

document.addEventListener("DOMContentLoaded", function () {
  const date = new Date();
  const fullDate = `Dzisiaj: ${date.getDate()}/${date.getMonth() + 1}/${date.getFullYear()}`;

  document.querySelectorAll('[data-bs-toggle="popover"]').forEach(label => {
    label.setAttribute("data-bs-content", fullDate);
    new bootstrap.Popover(label);
  });

  updateAllSpotColors();
});
