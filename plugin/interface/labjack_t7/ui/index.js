(() => {
  // interface/src/index.jsx
  var moduleUiPluginMeta = {
    moduleName: "labjack_t7",
    taskTypes: ["labjack.digital_write", "labjack.stream"]
  };
  function normalizeChannels(value) {
    if (Array.isArray(value)) {
      return value.join(", ");
    }
    return typeof value === "string" ? value : "";
  }
  function createModuleUiPlugin(host) {
    const React = host.React;
    const { useEffect, useState } = React;
    const {
      Button,
      Input,
      Label,
      Select,
      SelectContent,
      SelectItem,
      SelectTrigger,
      SelectValue,
      Separator,
      DialogDescription,
      DialogFooter,
      DialogHeader,
      DialogTitle
    } = host.components;
    function LabJackTaskSecondaryGui({ task }) {
      const args = task.arguments || {};
      const instanceName = args.module_instance_name || "UNBOUND";
      const channels = normalizeChannels(args.channels);
      return /* @__PURE__ */ React.createElement(React.Fragment, null, /* @__PURE__ */ React.createElement(Separator, null), /* @__PURE__ */ React.createElement("div", { className: "grid grid-cols-2 gap-2 text-xs" }, /* @__PURE__ */ React.createElement("div", { className: "min-w-0 rounded-md border bg-muted/40 px-3 py-2" }, /* @__PURE__ */ React.createElement("div", { className: "text-muted-foreground" }, "MODULE"), /* @__PURE__ */ React.createElement("div", { className: "truncate" }, instanceName)), /* @__PURE__ */ React.createElement("div", { className: "min-w-0 rounded-md border bg-muted/40 px-3 py-2" }, /* @__PURE__ */ React.createElement("div", { className: "text-muted-foreground" }, "CHANNELS"), /* @__PURE__ */ React.createElement("div", { className: "truncate" }, channels || "DEFAULT"))));
    }
    function useModuleInstances(operation) {
      const [moduleInstances, setModuleInstances] = useState([]);
      useEffect(() => {
        let active = true;
        async function loadModuleInstances() {
          const result = await operation("dartwic/get-module-instances", {
            registry_name: moduleUiPluginMeta.moduleName
          }, 15e3);
          if (active && !result?.error) {
            setModuleInstances(result?.payload?.module_instances || []);
          }
        }
        void loadModuleInstances();
        return () => {
          active = false;
        };
      }, [operation]);
      return moduleInstances;
    }
    function LabJackTaskDetailGui({ task, operation, onSaved, onClose }) {
      const moduleInstances = useModuleInstances(operation);
      const isStream = task.task_type === "labjack.stream";
      const [selectedInstance, setSelectedInstance] = useState(task.arguments?.module_instance_name || "");
      const [channels, setChannels] = useState(normalizeChannels(task.arguments?.channels) || (isStream ? "AIN0, AIN1" : "DIO8, DIO9, DIO10, DIO11"));
      const [targetScanRate, setTargetScanRate] = useState(String(task.arguments?.target_scan_rate ?? 100));
      const [scansPerRead, setScansPerRead] = useState(String(task.arguments?.scans_per_read ?? 10));
      const [errorMessage, setErrorMessage] = useState("");
      const [isSaving, setIsSaving] = useState(false);
      async function saveTask() {
        if (!selectedInstance) {
          setErrorMessage("SELECT A LABJACK MODULE INSTANCE.");
          return;
        }
        setIsSaving(true);
        setErrorMessage("");
        try {
          const cleanChannels = channels.split(",").map((item) => item.trim()).filter(Boolean);
          const argumentsPayload = {
            module_instance_name: selectedInstance,
            channels: isStream ? channels : cleanChannels
          };
          if (isStream) {
            argumentsPayload.target_scan_rate = Number(targetScanRate);
            argumentsPayload.scans_per_read = Number(scansPerRead);
          }
          const result = await operation("dartwic/create-task", {
            portal_name: task.portal,
            task_name: task.name,
            task_type: task.task_type,
            arguments: argumentsPayload
          }, 3e4);
          if (result?.error) {
            setErrorMessage((result?.payload?.error || "FAILED TO SAVE TASK.").toUpperCase());
            return;
          }
          if (onSaved) {
            await onSaved();
          }
          onClose?.();
        } finally {
          setIsSaving(false);
        }
      }
      return /* @__PURE__ */ React.createElement(React.Fragment, null, /* @__PURE__ */ React.createElement(DialogHeader, null, /* @__PURE__ */ React.createElement(DialogTitle, null, isStream ? "LABJACK STREAM TASK" : "LABJACK DIGITAL WRITE TASK"), /* @__PURE__ */ React.createElement(DialogDescription, null, isStream ? "BIND THIS WORKER TASK TO A LABJACK MODULE INSTANCE AND STREAM HARDWARE CHANNELS." : "BIND THIS PERIODIC TASK TO A LABJACK MODULE INSTANCE AND SYNC DIGITAL OUTPUT CHANNELS.")), /* @__PURE__ */ React.createElement("div", { className: "space-y-4 py-4" }, /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement(Label, null, "MODULE INSTANCE"), /* @__PURE__ */ React.createElement(Select, { value: selectedInstance, onValueChange: setSelectedInstance }, /* @__PURE__ */ React.createElement(SelectTrigger, { className: "w-full" }, /* @__PURE__ */ React.createElement(SelectValue, { placeholder: "SELECT A LABJACK MODULE INSTANCE" })), /* @__PURE__ */ React.createElement(SelectContent, null, moduleInstances.map((instance) => /* @__PURE__ */ React.createElement(SelectItem, { key: instance.name, value: instance.name }, instance.name))))), /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement(Label, null, isStream ? "STREAM CHANNELS" : "DIGITAL CHANNELS"), /* @__PURE__ */ React.createElement(
        Input,
        {
          value: channels,
          placeholder: isStream ? "AIN0, AIN1" : "DIO8, DIO9",
          onChange: (event) => setChannels(event.target.value)
        }
      )), isStream ? /* @__PURE__ */ React.createElement("div", { className: "grid grid-cols-2 gap-3" }, /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement(Label, null, "TARGET SCAN RATE"), /* @__PURE__ */ React.createElement(Input, { type: "number", min: "1", value: targetScanRate, onChange: (event) => setTargetScanRate(event.target.value) })), /* @__PURE__ */ React.createElement("div", { className: "space-y-2" }, /* @__PURE__ */ React.createElement(Label, null, "SCANS PER READ"), /* @__PURE__ */ React.createElement(Input, { type: "number", min: "1", value: scansPerRead, onChange: (event) => setScansPerRead(event.target.value) }))) : null, errorMessage ? /* @__PURE__ */ React.createElement("div", { className: "rounded-md border border-red-500/40 bg-red-500/10 px-3 py-2 text-sm text-red-200" }, errorMessage) : null), /* @__PURE__ */ React.createElement(DialogFooter, null, /* @__PURE__ */ React.createElement(Button, { variant: "ghost", onClick: onClose, disabled: isSaving }, "CANCEL"), /* @__PURE__ */ React.createElement(Button, { onClick: saveTask, disabled: isSaving }, isSaving ? "SAVING" : "SAVE")));
    }
    function LabJackModuleConfigPage({ instanceConfig, setInstanceConfig, save, moduleConfig = {} }) {
      const [isSaving, setIsSaving] = useState(false);
      const parameters = instanceConfig?.parameters || {};
      function updateParameterField(key, value) {
        setInstanceConfig((prev) => ({
          ...prev,
          parameters: {
            ...prev?.parameters || {},
            [key]: value
          }
        }));
      }
      async function handleSave() {
        setIsSaving(true);
        try {
          await save();
        } finally {
          setIsSaving(false);
        }
      }
      return /* @__PURE__ */ React.createElement("div", { className: "flex flex-col gap-4" }, /* @__PURE__ */ React.createElement("div", { className: "flex h-fit w-fit flex-row items-center gap-5 rounded-lg border border-border p-2" }, moduleConfig.icon || moduleConfig.icon_image_src ? /* @__PURE__ */ React.createElement("img", { className: "h-[30px]", src: moduleConfig.icon || moduleConfig.icon_image_src }) : null, /* @__PURE__ */ React.createElement(Label, { className: "text-lg" }, moduleConfig.title || "LabJack T7")), /* @__PURE__ */ React.createElement("div", { className: "flex h-fit w-fit flex-row justify-center gap-4" }, /* @__PURE__ */ React.createElement("div", { className: "flex flex-row items-center gap-5 rounded-lg border border-border p-2" }, /* @__PURE__ */ React.createElement(Label, null, instanceConfig?.name || "")), /* @__PURE__ */ React.createElement(Button, { variant: "outline", disabled: isSaving, onClick: handleSave }, isSaving ? "SAVING" : "SAVE CONFIG")), /* @__PURE__ */ React.createElement("div", { className: "flex flex-col gap-3 rounded-lg border border-border p-4" }, /* @__PURE__ */ React.createElement(Label, { className: "text-md font-semibold" }, "Connection"), [
        ["device_type", "Device Type", "T7"],
        ["connection_type", "Connection Type", "ANY"],
        ["identifier", "Identifier", "ANY or LJM_DEMO_MODE"],
        ["default_stream_channels", "Default Stream Channels", "AIN0, AIN1"]
      ].map(([key, label, placeholder]) => /* @__PURE__ */ React.createElement("div", { key, className: "flex flex-col gap-1" }, /* @__PURE__ */ React.createElement(Label, null, label), /* @__PURE__ */ React.createElement(
        Input,
        {
          value: parameters[key] ?? "",
          placeholder,
          onChange: (event) => updateParameterField(key, event.target.value)
        }
      )))));
    }
    return {
      id: moduleUiPluginMeta.moduleName,
      moduleName: moduleUiPluginMeta.moduleName,
      taskTypes: moduleUiPluginMeta.taskTypes,
      ModuleConfigPage: LabJackModuleConfigPage,
      TaskSecondaryGui: LabJackTaskSecondaryGui,
      TaskDetailGui: LabJackTaskDetailGui
    };
  }

  // interface/src/runtime-entry.jsx
  (function registerModuleUiPlugin() {
    const globalRegistry = window.__dartwicPluginRegistry__ = window.__dartwicPluginRegistry__ || {};
    globalRegistry[moduleUiPluginMeta.moduleName] = {
      createPlugin: createModuleUiPlugin
    };
  })();
})();
