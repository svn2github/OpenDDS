package DDS;

public class GuardCondition extends _GuardConditionInterfTAOPeer {

    public GuardCondition() {
        super(_jni_init());
    }

    private static native long _jni_init();
}
