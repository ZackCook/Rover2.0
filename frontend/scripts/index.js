document.addEventListener("DOMContentLoaded", () => {
  const ws = new WebSocket(`ws://${window.location.host}`);

  let assignedId = null;

  //DOM components
  const forwardButton = document.getElementById("forwardButton");

  // ******************************** /
  // ** DOM COMPONENT EVENT HANDLERS ** /
  // ******************************** /
  forwardButton.onmousedown = function () {
    logTimestamp("fwd pressed");
  };

  forwardButton.onmouseup = function () {
    logTimestamp("fwd released");
  };

  // ******************************** /
  // ** WEBSOCKETS EVENT HANDLERS ** /
  // ******************************** /

  ws.addEventListener("open", (event) => {
    logTimestamp(`Connected to server`);

    //send verification data
    const verifyMessage = createV2Message("verification", { clientType: "ui" }, "server-main");
    ws.send(JSON.stringify(verifyMessage));
    logTimestamp("Sent verification message to server");
  });

  ws.addEventListener("close", (event) => {
    logTimestamp("Disconnected from server");
  });

  ws.addEventListener("message", (event) => {
    logTimestamp(`Message from server: ${event.data}`);
  });

  // *********************** /
  // ** SUPPORT FUNCTIONS ** /
  // *********************** /
  function getCurrentDateTime() {
    return new Date().toISOString();
  }

  function logTimestamp(s) {
    console.log(`${s} @: ${getCurrentDateTime()}`);
  }

  function createV2Message(type, payload, target = null) {
    return {
      msgID: crypto.randomUUID(),
      msgType: type,
      msgSource: assignedId,
      msgTarget: target,
      msgTimestamp: new Date().toISOString(),
      msgPayload: payload,
    };
  }
});
