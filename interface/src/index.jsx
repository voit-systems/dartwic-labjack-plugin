export const moduleUiPluginMeta = {
    moduleName: "labjack_t7",
    taskTypes: ["labjack.digital_write", "labjack.stream"]
};

function normalizeChannels(value) {
    if (Array.isArray(value)) {
        return value.join(", ");
    }
    return typeof value === "string" ? value : "";
}

export function createModuleUiPlugin(host) {
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

        return (
            <>
                <Separator />
                <div className="grid grid-cols-2 gap-2 text-xs">
                    <div className="min-w-0 rounded-md border bg-muted/40 px-3 py-2">
                        <div className="text-muted-foreground">MODULE</div>
                        <div className="truncate">{instanceName}</div>
                    </div>
                    <div className="min-w-0 rounded-md border bg-muted/40 px-3 py-2">
                        <div className="text-muted-foreground">CHANNELS</div>
                        <div className="truncate">{channels || "DEFAULT"}</div>
                    </div>
                </div>
            </>
        );
    }

    function useModuleInstances(operation) {
        const [moduleInstances, setModuleInstances] = useState([]);

        useEffect(() => {
            let active = true;
            async function loadModuleInstances() {
                const result = await operation("dartwic/get-module-instances", {
                    registry_name: moduleUiPluginMeta.moduleName
                }, 15000);
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
                }, 30000);

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

        return (
            <>
                <DialogHeader>
                    <DialogTitle>{isStream ? "LABJACK STREAM TASK" : "LABJACK DIGITAL WRITE TASK"}</DialogTitle>
                    <DialogDescription>
                        {isStream ? "BIND THIS WORKER TASK TO A LABJACK MODULE INSTANCE AND STREAM HARDWARE CHANNELS." : "BIND THIS PERIODIC TASK TO A LABJACK MODULE INSTANCE AND SYNC DIGITAL OUTPUT CHANNELS."}
                    </DialogDescription>
                </DialogHeader>
                <div className="space-y-4 py-4">
                    <div className="space-y-2">
                        <Label>MODULE INSTANCE</Label>
                        <Select value={selectedInstance} onValueChange={setSelectedInstance}>
                            <SelectTrigger className="w-full">
                                <SelectValue placeholder="SELECT A LABJACK MODULE INSTANCE" />
                            </SelectTrigger>
                            <SelectContent>
                                {moduleInstances.map((instance) => (
                                    <SelectItem key={instance.name} value={instance.name}>
                                        {instance.name}
                                    </SelectItem>
                                ))}
                            </SelectContent>
                        </Select>
                    </div>
                    <div className="space-y-2">
                        <Label>{isStream ? "STREAM CHANNELS" : "DIGITAL CHANNELS"}</Label>
                        <Input
                            value={channels}
                            placeholder={isStream ? "AIN0, AIN1" : "DIO8, DIO9"}
                            onChange={(event) => setChannels(event.target.value)}
                        />
                    </div>
                    {isStream ? (
                        <div className="grid grid-cols-2 gap-3">
                            <div className="space-y-2">
                                <Label>TARGET SCAN RATE</Label>
                                <Input type="number" min="1" value={targetScanRate} onChange={(event) => setTargetScanRate(event.target.value)} />
                            </div>
                            <div className="space-y-2">
                                <Label>SCANS PER READ</Label>
                                <Input type="number" min="1" value={scansPerRead} onChange={(event) => setScansPerRead(event.target.value)} />
                            </div>
                        </div>
                    ) : null}
                    {errorMessage ? (
                        <div className="rounded-md border border-red-500/40 bg-red-500/10 px-3 py-2 text-sm text-red-200">
                            {errorMessage}
                        </div>
                    ) : null}
                </div>
                <DialogFooter>
                    <Button variant="ghost" onClick={onClose} disabled={isSaving}>
                        CANCEL
                    </Button>
                    <Button onClick={saveTask} disabled={isSaving}>
                        {isSaving ? "SAVING" : "SAVE"}
                    </Button>
                </DialogFooter>
            </>
        );
    }

    function LabJackModuleConfigPage({ instanceConfig, setInstanceConfig, save, moduleConfig = {} }) {
        const [isSaving, setIsSaving] = useState(false);
        const parameters = instanceConfig?.parameters || {};

        function updateParameterField(key, value) {
            setInstanceConfig((prev) => ({
                ...prev,
                parameters: {
                    ...(prev?.parameters || {}),
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

        return (
            <div className="flex flex-col gap-4">
                <div className="flex h-fit w-fit flex-row items-center gap-5 rounded-lg border border-border p-2">
                    {(moduleConfig.icon || moduleConfig.icon_image_src) ? (
                        <img className="h-[30px]" src={moduleConfig.icon || moduleConfig.icon_image_src} />
                    ) : null}
                    <Label className="text-lg">{moduleConfig.title || "LabJack T7"}</Label>
                </div>
                <div className="flex h-fit w-fit flex-row justify-center gap-4">
                    <div className="flex flex-row items-center gap-5 rounded-lg border border-border p-2">
                        <Label>{instanceConfig?.name || ""}</Label>
                    </div>
                    <Button variant="outline" disabled={isSaving} onClick={handleSave}>
                        {isSaving ? "SAVING" : "SAVE CONFIG"}
                    </Button>
                </div>
                <div className="flex flex-col gap-3 rounded-lg border border-border p-4">
                    <Label className="text-md font-semibold">Connection</Label>
                    {[
                        ["device_type", "Device Type", "T7"],
                        ["connection_type", "Connection Type", "ANY"],
                        ["identifier", "Identifier", "ANY or LJM_DEMO_MODE"],
                        ["default_stream_channels", "Default Stream Channels", "AIN0, AIN1"]
                    ].map(([key, label, placeholder]) => (
                        <div key={key} className="flex flex-col gap-1">
                            <Label>{label}</Label>
                            <Input
                                value={parameters[key] ?? ""}
                                placeholder={placeholder}
                                onChange={(event) => updateParameterField(key, event.target.value)}
                            />
                        </div>
                    ))}
                </div>
            </div>
        );
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
