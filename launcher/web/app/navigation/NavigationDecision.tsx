import { useEffect, useRef, useSyncExternalStore } from 'react';
import { FullscreenTransient } from '../components/FullscreenTransient';
import { ConfirmationDialog } from '../components/ConfirmationDialog';
import { useLocale } from '../i18n';
import type { DecisionStore } from '../models/decisions';
const noSubscription = () => () => {};
const noSnapshot = () => null;

/** One dialog presentation for application decisions and touch draft departure. */
export function NavigationDecision({
  decisions,
  blocked,
  onCancel,
  onConfirm,
}: {
  decisions?: DecisionStore;
  blocked: boolean;
  onCancel(): void;
  onConfirm(): void;
}) {
  const { t } = useLocale();
  const application = useSyncExternalStore(
    decisions?.subscribe ?? noSubscription,
    decisions?.getSnapshot ?? noSnapshot,
    decisions?.getSnapshot ?? noSnapshot,
  );
  const cancel = useRef(onCancel);
  cancel.current = onCancel;
  useEffect(() => {
    if (application && blocked) cancel.current();
    decisions?.setNavigationDecisionOpen(!application && blocked);
    return () => decisions?.setNavigationDecisionOpen(false);
  }, [application, blocked, decisions]);
  return (
    <FullscreenTransient>
      <ConfirmationDialog
        key={application ? 'application:' + application.requestId : 'navigation'}
        open={application !== null || blocked}
        title={application?.title || t('dialog.confirmTitle')}
        message={application ? application.message || '' : t('touch.layoutDiscardConfirm')}
        confirmText={
          application ? application.confirmText || t('action.confirm') : t('touch.discardChanges')
        }
        cancelText={application?.cancelText || t('action.cancel')}
        secondaryText={application?.secondaryText || ''}
        tone={application ? application.tone || 'normal' : 'danger'}
        variant={application?.variant || ''}
        hideCancel={application?.hideCancel || false}
        confirmOnEnter={application?.confirmOnEnter || false}
        onSecondary={() => {
          if (application) decisions?.resolve('secondary');
        }}
        onCancel={() => {
          if (application) decisions?.resolve('cancel');
          else onCancel();
        }}
        onConfirm={() => {
          if (application) decisions?.resolve('confirm');
          else onConfirm();
        }}
      />
    </FullscreenTransient>
  );
}
